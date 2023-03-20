#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/InstIterator.h"
#include <map>

#include "LLAMX.h"

using namespace llvm;

namespace {
struct AMXLowering : public PassInfoMixin<AMXLowering> {
  PreservedAnalyses run(Function &, FunctionAnalysisManager &);
};
struct AMXLowLevelRegAlloc : public PassInfoMixin<AMXLowLevelRegAlloc> {
  PreservedAnalyses run(Function &, FunctionAnalysisManager &);
};
} // namespace

static void buildPasses(PassBuilder &PB) {
  PB.registerPipelineParsingCallback(
      [](StringRef Name, FunctionPassManager &FPM,
         ArrayRef<PassBuilder::PipelineElement>) {
        if (Name == "low-level-regalloc") {
          FPM.addPass(AMXLowLevelRegAlloc());
          return true;
        }
        if (Name == "lower-amx") {
          FPM.addPass(AMXLowering());
          return true;
        }
        return false;
      });

  PB.registerScalarOptimizerLateEPCallback(
      [](FunctionPassManager &FPM, OptimizationLevel) {
        FPM.addPass(AMXLowLevelRegAlloc());
        FPM.addPass(AMXLowering());
      });
}

static Value *emitConfig(std::list<std::pair<Value*, int>> inputs, IRBuilderBase &IRB) {
  auto *Int64Ty = IRB.getInt64Ty();
  Value *out = IRB.getInt64(0);
  for (auto &input : inputs) {
    out = IRB.CreateOr(out, IRB.CreateShl(input.first, input.second));
  }
  return out;
}

static Value *emitLoadStoreConfig(Value *Ptr, Value *Reg, IRBuilderBase &IRB) {
  auto *Int64Ty = IRB.getInt64Ty();
  auto *Addr = IRB.CreatePointerCast(Ptr, Int64Ty);
  auto *RegNum = IRB.CreateZExt(Reg, Int64Ty);
  auto *Operand = IRB.CreateOr(Addr, IRB.CreateShl(RegNum, 56), "myor");
  return Operand;
}

static Value *emitFMAConfig(Value *XReg, Value *YReg, Value *ZReg, 
                            int OpType, int Mode, IRBuilderBase &IRB) {
  auto *Int64Ty = IRB.getInt64Ty();
  auto *XRegNum = IRB.CreateZExt(XReg, Int64Ty);
  auto *YRegNum = IRB.CreateZExt(YReg, Int64Ty);
  auto *ZRegNum = IRB.CreateZExt(ZReg, Int64Ty);
  Value *OpTypeNum = IRB.getInt64(OpType);
  Value *ModeNum = IRB.getInt64(Mode);
  
  return emitConfig({{XRegNum, 16}, {YRegNum, 6}, {ZRegNum, 20}, {OpTypeNum, 27}, {ModeNum, 63}}, IRB);
}

static Value *emitExtrConfig(Value *DstReg, Value *SrcReg, int isDestY, int isSrcXY, IRBuilderBase &IRB) {
  auto *Int64Ty = IRB.getInt64Ty();
  auto *DstRegNum = IRB.CreateZExt(DstReg, Int64Ty);
  auto *SrcRegNum = IRB.CreateZExt(SrcReg, Int64Ty);
  Value *isSrcXYNum = IRB.getInt64(isSrcXY);
  
  return emitConfig({{DstRegNum, (isDestY ? 6 : 16)}, {SrcRegNum, 20}, {isSrcXYNum, 27}}, IRB);
}

static void emitAMX(AMXOpcode Opcode, Value *Operand, IRBuilderBase &IRB) {
  auto *VoidTy = IRB.getVoidTy();
  auto *Int32Ty = IRB.getInt32Ty();
  auto *Int64Ty = IRB.getInt64Ty();
  auto *FnTy = FunctionType::get(VoidTy, {Int32Ty, Int64Ty}, false);
  InlineAsm *IA = InlineAsm::get(
      FnTy, ".word (0x201000 + ($0 << 5) + 0$1 - ((0$1 >> 4) * 6))",
      "i,r,~{memory}", true /* hasSideEffect */);
  IRB.CreateCall(IA, {ConstantInt::get(Int32Ty, Opcode), Operand});
}

static void lowerLoad(CallInst *CI, Instruction *InsertBefore, AMXOpcode Opcode) {
  auto *DestReg = CI->getArgOperand(0);
  auto *SrcPtr = CI->getArgOperand(1);

  assert(DestReg->getType()->isIntegerTy(32));
  // assert(isa<ConstantInt>(DestReg));
  assert(SrcPtr->getType()->isPointerTy());

  IRBuilder<> IRB(InsertBefore);

  emitAMX(Opcode, emitLoadStoreConfig(SrcPtr, DestReg, IRB), IRB);
}

static void lowerStore(CallInst *CI, Instruction *InsertBefore, AMXOpcode Opcode) {
  auto *DestPtr = CI->getArgOperand(0);
  auto *SrcReg = CI->getArgOperand(1);

  assert(SrcReg->getType()->isIntegerTy(32));
  // assert(isa<ConstantInt>(SrcReg));
  assert(DestPtr->getType()->isPointerTy());

  IRBuilder<> IRB(InsertBefore);

  emitAMX(Opcode, emitLoadStoreConfig(DestPtr, SrcReg, IRB), IRB);
}

static void lowerFloatOp(CallInst *CI, Instruction *InsertBefore, AMXOpcode Opcode, int ALUOp, int isVectorOp) {
  auto *XReg = CI->getArgOperand(1);
  auto *YReg = CI->getArgOperand(2);
  auto *ZReg = CI->getArgOperand(0);

  assert(XReg->getType()->isIntegerTy(32));
  // assert(isa<ConstantInt>(XReg));
  assert(YReg->getType()->isIntegerTy(32));
  // assert(isa<ConstantInt>(YReg));
  assert(ZReg->getType()->isIntegerTy(32));
  // assert(isa<ConstantInt>(ZReg));

  IRBuilder<> IRB(InsertBefore);

  emitAMX(Opcode, emitFMAConfig(
    XReg,
    YReg,
    ZReg,
    ALUOp, // bit27-29
    isVectorOp, // bit63
    IRB
  ), IRB);
}

static void lowerExtr(CallInst *CI, Instruction *InsertBefore, AMXOpcode Opcode, int isDestY, int isSrcXY) {
  auto *DstReg = CI->getArgOperand(0);
  auto *SrcReg = CI->getArgOperand(1);

  assert(DstReg->getType()->isIntegerTy(32));
  assert(SrcReg->getType()->isIntegerTy(32));

  IRBuilder<> IRB(InsertBefore);
  
  emitAMX(Opcode, emitExtrConfig(DstReg, SrcReg, isDestY, isSrcXY, IRB), IRB);
}

PreservedAnalyses AMXLowering::run(Function &F, FunctionAnalysisManager &AM) {
  errs() << "!!! processing " << F.getName() << '\n';
  std::vector<Instruction *> DeadInsts;
  for (auto &BB : F) {
    for (auto &I : BB) {
      auto *CI = dyn_cast<CallInst>(&I);
      if (!CI)
        continue;
      auto *Callee = CI->getCalledFunction();
      if (!Callee || !Callee->getName().startswith("amx_"))
        continue;

      // Loads and stores
      if (Callee->getName() == "amx_ldx") {
        lowerLoad(CI, &I, AMXOpcode::LDX);
      } else if (Callee->getName() == "amx_ldy") {
        lowerLoad(CI, &I, AMXOpcode::LDY);
      } else if (Callee->getName() == "amx_stx") {
        lowerStore(CI, &I, AMXOpcode::STX);
      } else if (Callee->getName() == "amx_sty") {
        lowerStore(CI, &I, AMXOpcode::STY);
      } else if (Callee->getName() == "amx_ldz") {
        lowerLoad(CI, &I, AMXOpcode::LDZ);
      } else if (Callee->getName() == "amx_stz") {
        lowerStore(CI, &I, AMXOpcode::STZ);
      } else if (Callee->getName() == "amx_ldzi") {
        lowerLoad(CI, &I, AMXOpcode::LDZI);
      } else if (Callee->getName() == "amx_stzi") {
        lowerStore(CI, &I, AMXOpcode::STZI);
      } 
      
      // Moves
      else if (Callee->getName() == "amx_mvxy") {
        lowerExtr(CI, &I, AMXOpcode::EXTRY, 1 /*isDestY*/, 1 /*IsSrcXY*/);
      } else if (Callee->getName() == "amx_mvyx") {
        lowerExtr(CI, &I, AMXOpcode::EXTRX, 0 /*isDestY*/, 1 /*IsSrcXY*/);
      } 

      // else if (Callee->getName() == "amx_mvxz") {
      //   lowerFloatOp(CI, &I, AMXOpcode::FMA64, 3 /*ALUOp*/, 1 /*isVectorOp*/); // crashes
      // } else if (Callee->getName() == "amx_mvyz") {
      //   lowerFloatOp(CI, &I, AMXOpcode::FMA64, 5 /*ALUOp*/, 1 /*isVectorOp*/); // crashes
      // } else if (Callee->getName() == "amx_mvzx") {
      //   lowerExtr(CI, &I, AMXOpcode::EXTRY, 0 /*isDestY*/, 0 /*IsSrcXY*/); // need extrv
      // } else if (Callee->getName() == "amx_mvzy") {
      //   lowerExtr(CI, &I, AMXOpcode::EXTRX, 1 /*isDestY*/, 0 /*IsSrcXY*/); // need extrv
      // } 
      
      // Floating-point ops
      else if (Callee->getName() == "amx_fma64_mat") {
        lowerFloatOp(CI, &I, AMXOpcode::FMA64, 0 /*ALUOp*/, 0 /*isVectorOp*/);
      } else if (Callee->getName() == "amx_fma32_mat") {
        lowerFloatOp(CI, &I, AMXOpcode::FMA32, 0 /*ALUOp*/, 0 /*isVectorOp*/);
      } else if (Callee->getName() == "amx_fma16_mat") {
        lowerFloatOp(CI, &I, AMXOpcode::FMA16, 0 /*ALUOp*/, 0 /*isVectorOp*/);
      } else if (Callee->getName() == "amx_fms64_mat") {
        lowerFloatOp(CI, &I, AMXOpcode::FMS64, 0 /*ALUOp*/, 0 /*isVectorOp*/);
      } else if (Callee->getName() == "amx_fms32_mat") {
        lowerFloatOp(CI, &I, AMXOpcode::FMS32, 0 /*ALUOp*/, 0 /*isVectorOp*/);
      } else if (Callee->getName() == "amx_fms16_mat") {
        lowerFloatOp(CI, &I, AMXOpcode::FMS16, 0 /*ALUOp*/, 0 /*isVectorOp*/);
      } else if (Callee->getName() == "amx_fma64_vec") {
        lowerFloatOp(CI, &I, AMXOpcode::FMA64, 0 /*ALUOp*/, 1 /*isVectorOp*/);
      } else if (Callee->getName() == "amx_fma32_vec") {
        lowerFloatOp(CI, &I, AMXOpcode::FMA32, 0 /*ALUOp*/, 1 /*isVectorOp*/);
      } else if (Callee->getName() == "amx_fma16_vec") {
        lowerFloatOp(CI, &I, AMXOpcode::FMA16, 0 /*ALUOp*/, 1 /*isVectorOp*/);
      } else if (Callee->getName() == "amx_fms64_vec") {
        lowerFloatOp(CI, &I, AMXOpcode::FMS64, 0 /*ALUOp*/, 1 /*isVectorOp*/);
      } else if (Callee->getName() == "amx_fms32_vec") {
        lowerFloatOp(CI, &I, AMXOpcode::FMS32, 0 /*ALUOp*/, 1 /*isVectorOp*/);
      } else if (Callee->getName() == "amx_fms16_vec") {
        lowerFloatOp(CI, &I, AMXOpcode::FMS16, 0 /*ALUOp*/, 1 /*isVectorOp*/);
      } else {
        llvm_unreachable("don't know how to lower this intrinsic");
      }

      DeadInsts.push_back(&I);
    }
  }

  for (auto *I : DeadInsts)
    I->eraseFromParent();

  errs() << F << '\n';
  return PreservedAnalyses::none();
}

namespace {

enum RegType {
  X, Y, Z, Z_PAIR, Z_COL, Z_COL2, Z_COL4, Z_COL8
};

struct OperandInfo {
  bool IsUsed;
  bool IsDefined;
  RegType Ty;

  static OperandInfo getDef(RegType Ty) {
    return {
      .IsUsed = false,
      .IsDefined = true,
      .Ty = Ty
    };
  }

  static OperandInfo getUse(RegType Ty) {
    return {
      .IsUsed = true,
      .IsDefined = false,
      .Ty = Ty
    };
  }
};

struct RegInfo {
  RegType Ty;
  unsigned Number;
  RegInfo(RegType Ty, unsigned Number) : Ty(Ty), Number(Number) {}
  bool operator<(const RegInfo &Other) const {
    return std::tie(Ty, Number) < std::tie(Other.Ty, Other.Number);
  }
  // TODO: this really depends on Ty
  unsigned size() {
    return 64;
  }
};

class RegisterSpiller {
  Function *F;
  // Mapping <virtual reg> -> <alloca>
  std::map<RegInfo, AllocaInst *> RegToMemMap;
public:
  RegisterSpiller(Function *F) : F(F) {}
  bool isSpilled(RegInfo Virt) const {
    return RegToMemMap.count(Virt);
  }
  void allocateMemory(RegInfo Virt) {
    assert(!RegToMemMap.count(Virt));
    BasicBlock &Entry = F->getEntryBlock();
    auto &Ctx = F->getContext();
    auto *Int8Ty = Type::getInt8Ty(Ctx);
    auto *Int64Ty = Type::getInt64Ty(Ctx);
    auto *Alloca = new AllocaInst(Int8Ty, 0, ConstantInt::get(Int64Ty, Virt.size()), "amx.spilled", Entry.getFirstNonPHI());
    RegToMemMap[Virt] = Alloca;
  }

  void store(RegInfo Phys, RegInfo Virt, IRBuilderBase &IRB) {
    auto Ty = Phys.Ty;
    assert(Ty == Virt.Ty);
    assert(RegToMemMap.count(Virt));
    auto *Ptr = RegToMemMap[Virt];
    auto &Ctx = F->getContext();
    auto *Int64Ty = Type::getInt64Ty(Ctx);
    if (Ty == RegType::X) {
      emitAMX(AMXOpcode::STX, emitLoadStoreConfig(Ptr, ConstantInt::get(Int64Ty, Phys.Number), IRB), IRB);
      return;
    }
    if (Ty == RegType::Y) {
      emitAMX(AMXOpcode::STY, emitLoadStoreConfig(Ptr, ConstantInt::get(Int64Ty, Phys.Number), IRB), IRB);
      return;
    }
    if (Ty == RegType::Z) {
      emitAMX(AMXOpcode::STZ, emitLoadStoreConfig(Ptr, ConstantInt::get(Int64Ty, Phys.Number), IRB), IRB);
      return;
    }
    llvm_unreachable("don't know how to store this register type");
  }

  // Reload a the virtual register onto the specified physical reg.
  void reload(RegInfo Phys, RegInfo Virt, IRBuilderBase &IRB) {
    auto Ty = Phys.Ty;
    assert(Ty == Virt.Ty);
    assert(RegToMemMap.count(Virt));
    auto *Ptr = RegToMemMap[Virt];
    auto &Ctx = F->getContext();
    auto *Int64Ty = Type::getInt64Ty(Ctx);
    if (Ty == RegType::X) {
      emitAMX(AMXOpcode::LDX, emitLoadStoreConfig(Ptr, ConstantInt::get(Int64Ty, Phys.Number), IRB), IRB);
      return;
    }
    if (Ty == RegType::Y) {
      emitAMX(AMXOpcode::LDY, emitLoadStoreConfig(Ptr, ConstantInt::get(Int64Ty, Phys.Number), IRB), IRB);
      return;
    }
    if (Ty == RegType::Z) {
      emitAMX(AMXOpcode::LDZ, emitLoadStoreConfig(Ptr, ConstantInt::get(Int64Ty, Phys.Number), IRB), IRB);
      return;
    }
    llvm_unreachable("don't know how to store this register type");
  }
};

} // namespace

// Given an LLAMX operation, returns the OperandInfo of the operands
SmallVector<Optional<OperandInfo>, 4> getRegInfos(CallInst *CI) {
  auto *Callee = CI->getCalledFunction();
  if (!Callee)
    return {};
  if (Callee->getName() == "amx_ldx")
    return {OperandInfo::getDef(RegType::X), None};
  if (Callee->getName() == "amx_ldy")
    return {OperandInfo::getDef(RegType::Y), None};
  if (Callee->getName() == "amx_stx")
    return {None, OperandInfo::getUse(RegType::X)};
  if (Callee->getName() == "amx_sty")
    return {None, OperandInfo::getUse(RegType::Y)};
  if (Callee->getName() == "amx_ldz")
    return {OperandInfo::getDef(RegType::Z), None};
  if (Callee->getName() == "amx_stz")
    return {None, OperandInfo::getUse(RegType::Z)};
  if (Callee->getName() == "amx_ldzi")
    return {OperandInfo::getDef(RegType::Z_PAIR), None};
  if (Callee->getName() == "amx_stzi")
    return {None, OperandInfo::getUse(RegType::Z_PAIR)};
  return {};
}

void setRegs(CallInst *CI, ArrayRef<Optional<OperandInfo>> RegInfos,
             ArrayRef<unsigned> PhysRegs) {
  assert(RegInfos.size() == PhysRegs.size());
  assert(CI->arg_size() == RegInfos.size());
  for (auto [Info, PhysReg, Operand] : llvm::zip(RegInfos, PhysRegs, CI->args())) {
    if (!Info)
      continue;
    auto *Ty = Operand.get()->getType();
    Operand.set(ConstantInt::get(Ty, PhysReg));
  }
}

unsigned getConstValue(Value *V) {
  return cast<ConstantInt>(V)->getZExtValue();
}

PreservedAnalyses AMXLowLevelRegAlloc::run(Function &F, FunctionAnalysisManager &AM) {
  RegisterSpiller Spiller(&F);
  std::vector<Instruction *> Insts;
  for (auto &I : instructions(F))
    Insts.push_back(&I);

  for (auto *I : Insts) {
    auto *CI = dyn_cast<CallInst>(I);
    if (!CI)
      continue;
    auto Infos = getRegInfos(CI);
    // don't need to do anything if the instruction doesn't use any AMX registers
    if (Infos.empty())
      continue;

    for (auto [Info, V] : zip(Infos, CI->args())) {
      if (!Info)
        continue;
      RegInfo Virt(Info->Ty, getConstValue(V));
      if (!Spiller.isSpilled(Virt))
        Spiller.allocateMemory(Virt);
    }

    IRBuilder<> IRB(CI);

    // Reload the used registers
    for (auto [Info, V] : zip(Infos, CI->args())) {
      if (!Info || !Info->IsUsed)
        continue;
      RegInfo Virt(Info->Ty, getConstValue(V));
      RegInfo Phys(Info->Ty, 0);
      assert(Spiller.isSpilled(Virt));
      Spiller.reload(Phys, Virt, IRB);
    }

    // Store the defined registers
    IRB.SetInsertPoint(I->getParent(), std::next(I->getIterator()));
    for (auto [Info, V] : zip(Infos, CI->args())) {
      if (!Info || !Info->IsDefined)
        continue;
      RegInfo Virt(Info->Ty, getConstValue(V));
      RegInfo Phys(Info->Ty, 0);
      assert(Spiller.isSpilled(Virt));
      Spiller.store(Phys, Virt, IRB);
    }

    SmallVector<unsigned> PhysRegs(Infos.size(), 0);
    setRegs(CI, Infos, PhysRegs);
  }

  errs() << "!!! done with register allocation\n";
  errs() << F << '\n';
  return PreservedAnalyses::none();
}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "llamx", LLVM_VERSION_STRING, buildPasses};
}
