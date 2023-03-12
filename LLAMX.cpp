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

static Value *emitLoadStoreConfig(Value *Ptr, Constant *Reg,
                                  IRBuilderBase &IRB) {
  auto *Int64Ty = IRB.getInt64Ty();
  auto *Addr = IRB.CreatePointerCast(Ptr, Int64Ty);
  auto *RegNum = IRB.CreateZExt(Reg, Int64Ty);
  auto *Operand = IRB.CreateOr(Addr, IRB.CreateShl(RegNum, 56), "myor");
  return Operand;
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

static void lowerLDX(Constant *DestReg, Value *Ptr, Instruction *InsertBefore) {
  errs() << "lowering LDX: dest reg = " << *DestReg << ", ptr = " << *Ptr
         << '\n';
  IRBuilder<> IRB(InsertBefore);
  emitAMX(AMXOpcode::LDX, emitLoadStoreConfig(Ptr, DestReg, IRB), IRB);
}

static void lowerLDY(Constant *DestReg, Value *Ptr, Instruction *InsertBefore) {
  errs() << "lowering LDY: dest reg = " << *DestReg << ", ptr = " << *Ptr
         << '\n';
  IRBuilder<> IRB(InsertBefore);
  emitAMX(AMXOpcode::LDY, emitLoadStoreConfig(Ptr, DestReg, IRB), IRB);
}

static void lowerSTX(Value *Ptr, Constant *SrcReg, Instruction *InsertBefore) {
  errs() << "lowering STX: dest ptr = " << *Ptr << ", src reg = " << *SrcReg
         << '\n';
  IRBuilder<> IRB(InsertBefore);
  emitAMX(AMXOpcode::STX, emitLoadStoreConfig(Ptr, SrcReg, IRB), IRB);
}

static void lowerSTY(Value *Ptr, Constant *SrcReg, Instruction *InsertBefore) {
  errs() << "lowering STY: dest ptr = " << *Ptr << ", src reg = " << *SrcReg
         << '\n';
  IRBuilder<> IRB(InsertBefore);
  emitAMX(AMXOpcode::STY, emitLoadStoreConfig(Ptr, SrcReg, IRB), IRB);
}

static void lowerLDZ(Constant *DestReg, Value *Ptr, Instruction *InsertBefore) {
  errs() << "lowering LDZ: dest reg = " << *DestReg << ", ptr = " << *Ptr
         << '\n';
  IRBuilder<> IRB(InsertBefore);
  emitAMX(AMXOpcode::LDZ, emitLoadStoreConfig(Ptr, DestReg, IRB), IRB);
}

static void lowerSTZ(Value *Ptr, Constant *SrcReg, Instruction *InsertBefore) {
  errs() << "lowering STZ: dest ptr = " << *Ptr << ", src reg = " << *SrcReg
         << '\n';
  IRBuilder<> IRB(InsertBefore);
  emitAMX(AMXOpcode::STZ, emitLoadStoreConfig(Ptr, SrcReg, IRB), IRB);
}

static void lowerLDZI(Constant *DestReg, Value *Ptr, Instruction *InsertBefore) {
  errs() << "lowering LDZ: dest reg/half = " << *DestReg << ", ptr = " << *Ptr
         << '\n';
  IRBuilder<> IRB(InsertBefore);
  emitAMX(AMXOpcode::LDZI, emitLoadStoreConfig(Ptr, DestReg, IRB), IRB);
}

static void lowerSTZI(Value *Ptr, Constant *SrcReg, Instruction *InsertBefore) {
  errs() << "lowering STZ: dest ptr = " << *Ptr << ", src reg/half = " << *SrcReg
         << '\n';
  IRBuilder<> IRB(InsertBefore);
  emitAMX(AMXOpcode::STZI, emitLoadStoreConfig(Ptr, SrcReg, IRB), IRB);
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
        auto *DestReg = CI->getArgOperand(0);
        auto *Ptr = CI->getArgOperand(1);

        assert(DestReg->getType()->isIntegerTy(32));
        assert(isa<ConstantInt>(DestReg));
        assert(Ptr->getType()->isPointerTy());

        lowerLDX(cast<ConstantInt>(DestReg), Ptr, &I /*insert before*/);
      } else if (Callee->getName() == "amx_ldy") {
        auto *DestReg = CI->getArgOperand(0);
        auto *Ptr = CI->getArgOperand(1);

        assert(DestReg->getType()->isIntegerTy(32));
        assert(isa<ConstantInt>(DestReg));
        assert(Ptr->getType()->isPointerTy());

        lowerLDY(cast<ConstantInt>(DestReg), Ptr, &I /*insert before*/);
      } else if (Callee->getName() == "amx_stx") {
        auto *DestPtr = CI->getArgOperand(0);
        auto *SrcReg = CI->getArgOperand(1);

        assert(SrcReg->getType()->isIntegerTy(32));
        assert(isa<ConstantInt>(SrcReg));

        lowerSTX(DestPtr, cast<ConstantInt>(SrcReg), &I /*insert before*/);
      } else if (Callee->getName() == "amx_sty") {
        auto *DestPtr = CI->getArgOperand(0);
        auto *SrcReg = CI->getArgOperand(1);

        assert(SrcReg->getType()->isIntegerTy(32));
        assert(isa<ConstantInt>(SrcReg));

        lowerSTY(DestPtr, cast<ConstantInt>(SrcReg), &I /*insert before*/);
      } else if (Callee->getName() == "amx_ldz") {
        auto *DestReg = CI->getArgOperand(0);
        auto *Ptr = CI->getArgOperand(1);

        assert(DestReg->getType()->isIntegerTy(32));
        assert(isa<ConstantInt>(DestReg));
        assert(Ptr->getType()->isPointerTy());

        lowerLDZ(cast<ConstantInt>(DestReg), Ptr, &I /*insert before*/);
      } else if (Callee->getName() == "amx_stz") {
        auto *DestPtr = CI->getArgOperand(0);
        auto *SrcReg = CI->getArgOperand(1);

        assert(SrcReg->getType()->isIntegerTy(32));
        assert(isa<ConstantInt>(SrcReg));

        lowerSTZ(DestPtr, cast<ConstantInt>(SrcReg), &I /*insert before*/);
      } else if (Callee->getName() == "amx_ldzi") {
        auto *DestReg = CI->getArgOperand(0);
        auto *Ptr = CI->getArgOperand(1);

        assert(DestReg->getType()->isIntegerTy(32));
        assert(isa<ConstantInt>(DestReg));
        assert(Ptr->getType()->isPointerTy());

        lowerLDZI(cast<ConstantInt>(DestReg), Ptr, &I /*insert before*/);
      } else if (Callee->getName() == "amx_stzi") {
        auto *DestPtr = CI->getArgOperand(0);
        auto *SrcReg = CI->getArgOperand(1);

        assert(SrcReg->getType()->isIntegerTy(32));
        assert(isa<ConstantInt>(SrcReg));

        lowerSTZI(DestPtr, cast<ConstantInt>(SrcReg), &I /*insert before*/);
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
