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

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "llamx", LLVM_VERSION_STRING, buildPasses};
}
