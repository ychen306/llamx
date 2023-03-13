#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"

#include "LLAMX.h"

using namespace llvm;

namespace {
struct AMXLowering : public PassInfoMixin<AMXLowering> {
  PreservedAnalyses run(Function &, FunctionAnalysisManager &);
};
} // namespace

static void buildPasses(PassBuilder &PB) {
  PB.registerPipelineParsingCallback(
      [](StringRef Name, FunctionPassManager &FPM,
         ArrayRef<PassBuilder::PipelineElement>) {
        if (Name == "lower-amx") {
          FPM.addPass(AMXLowering());
          return true;
        }
        return false;
      });

  PB.registerScalarOptimizerLateEPCallback(
      [](FunctionPassManager &FPM, OptimizationLevel) {
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

  emitAMX(Opcode, emitLoadStoreConfig(SrcPtr, (DestReg), IRB), IRB);
}

static void lowerStore(CallInst *CI, Instruction *InsertBefore, AMXOpcode Opcode) {
  auto *DestPtr = CI->getArgOperand(0);
  auto *SrcReg = CI->getArgOperand(1);

  assert(SrcReg->getType()->isIntegerTy(32));
  // assert(isa<ConstantInt>(SrcReg));
  assert(DestPtr->getType()->isPointerTy());

  IRBuilder<> IRB(InsertBefore);

  emitAMX(Opcode, emitLoadStoreConfig(DestPtr, (SrcReg), IRB), IRB);
}

static void lowerFma(CallInst *CI, Instruction *InsertBefore, AMXOpcode Opcode) {
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
    (XReg),
    (YReg),
    (ZReg),
    0, // bit27-29
    0, // bit63
    IRB
  ), IRB);
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
      } else if (Callee->getName() == "amx_fma64") {
        lowerFma(CI, &I, AMXOpcode::FMA64);
      } else if (Callee->getName() == "amx_fma32") {
        lowerFma(CI, &I, AMXOpcode::FMA32);
      } else if (Callee->getName() == "amx_fma16") {
        lowerFma(CI, &I, AMXOpcode::FMA16);
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

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "llamx", LLVM_VERSION_STRING, buildPasses};
}
