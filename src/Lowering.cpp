#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/InstIterator.h"
#include <map>

#include "LLAMX.h"
#include "Lowering.h"

using namespace llvm;

Value *emitConfig(std::list<std::pair<Value*, int>> inputs, IRBuilderBase &IRB) {
  auto *Int64Ty = IRB.getInt64Ty();
  Value *out = IRB.getInt64(0);
  for (auto &input : inputs) {
    out = IRB.CreateOr(out, IRB.CreateShl(input.first, input.second));
  }
  return out;
}

Value *emitLoadStoreConfig(Value *Ptr, Value *Reg, IRBuilderBase &IRB) {
  auto *Int64Ty = IRB.getInt64Ty();
  auto *Addr = IRB.CreatePointerCast(Ptr, Int64Ty);
  auto *RegNum = IRB.CreateZExt(Reg, Int64Ty);
  auto *Operand = IRB.CreateOr(Addr, IRB.CreateShl(RegNum, 56));
  return Operand;
}

Value *emitFMAConfig(Value *XReg, Value *YReg, Value *ZReg, 
                            int OpType, int Mode, IRBuilderBase &IRB) {
  auto *Int64Ty = IRB.getInt64Ty();
  auto *XRegNum = IRB.CreateZExt(XReg, Int64Ty);
  auto *YRegNum = IRB.CreateZExt(YReg, Int64Ty);
  auto *ZRegNum = IRB.CreateZExt(ZReg, Int64Ty);
  Value *OpTypeNum = IRB.getInt64(OpType);
  Value *ModeNum = IRB.getInt64(Mode);
  
  return emitConfig({{XRegNum, 16}, {YRegNum, 6}, {ZRegNum, 20}, {OpTypeNum, 27}, {ModeNum, 63}}, IRB);
}

Value *emitExtrConfig(Value *DstReg, Value *SrcReg, int isDestY, int isSrcXY, IRBuilderBase &IRB) {
  auto *Int64Ty = IRB.getInt64Ty();
  auto *DstRegNum = IRB.CreateZExt(DstReg, Int64Ty);
  auto *SrcRegNum = IRB.CreateZExt(SrcReg, Int64Ty);
  Value *isSrcXYNum = IRB.getInt64(isSrcXY);
  Value *isDestYNum = IRB.getInt64(isDestY);
  Value *Const1Num = IRB.getInt64(1);

  if (isSrcXY) {
    return emitConfig({{DstRegNum, (isDestY ? 6 : 16)}, {SrcRegNum, 20}, {isSrcXYNum, 27}}, IRB);
  } else {
    return emitConfig({{DstRegNum, 3}, {isDestYNum, 10}, {SrcRegNum, 20}, {Const1Num, 26}}, IRB);
  }
}

Value *emitExtrhConfig(Value *DstReg, Value *SrcReg, int isDestY, int isSrcXY, IRBuilderBase &IRB) {
  auto *Int64Ty = IRB.getInt64Ty();
  auto *DstRegNum = IRB.CreateZExt(DstReg, Int64Ty);
  auto *SrcRegNum = IRB.CreateZExt(SrcReg, Int64Ty);
  Value *isSrcXYNum = IRB.getInt64(isSrcXY);
  
  return emitConfig({{DstRegNum, (isDestY ? 6 : 16)}, {SrcRegNum, 20}, {isSrcXYNum, 27}}, IRB);
}

void emitAMX(AMXOpcode Opcode, Value *Operand, IRBuilderBase &IRB) {
  auto *VoidTy = IRB.getVoidTy();
  auto *Int32Ty = IRB.getInt32Ty();
  auto *Int64Ty = IRB.getInt64Ty();
  auto *FnTy = FunctionType::get(VoidTy, {Int32Ty, Int64Ty}, false);
  InlineAsm *IA = InlineAsm::get(
      FnTy, ".word (0x201000 + ($0 << 5) + 0$1 - ((0$1 >> 4) * 6))",
      "i,r,~{memory}", true /* hasSideEffect */);
  IRB.CreateCall(IA, {ConstantInt::get(Int32Ty, Opcode), Operand});
}

void lowerLoad(CallInst *CI, Instruction *InsertBefore, AMXOpcode Opcode) {
  auto *DestReg = CI->getArgOperand(0);
  auto *SrcPtr = CI->getArgOperand(1);

  assert(DestReg->getType()->isIntegerTy(32));
  assert(isa<ConstantInt>(DestReg));
  assert(SrcPtr->getType()->isPointerTy());

  IRBuilder<> IRB(InsertBefore);

  emitAMX(Opcode, emitLoadStoreConfig(SrcPtr, DestReg, IRB), IRB);
}

// Lower loads of z columns. numCols must be 1, 2, 4, or 8.
void lowerLoadZCols(CallInst *CI, Instruction *InsertBefore, int numCols) {
  auto *DestCol = CI->getArgOperand(0);
  auto *SrcPtr = CI->getArgOperand(1);

  assert(DestCol->getType()->isIntegerTy(32));
  assert(isa<ConstantInt>(DestCol));
  assert(SrcPtr->getType()->isPointerTy());

  IRBuilder<> IRB(InsertBefore);

  int numRegs = 8 * numCols;
  int stride = 8 / numCols;

  auto *Int64Ty = IRB.getInt64Ty();
  auto* ArrayTy = ArrayType::get(Int64Ty, 8);

  for (int i = 0; i < numRegs; i++) {
    auto *DestReg = IRB.CreateAdd(DestCol, ConstantInt::get(Int64Ty, stride * i));
    ArrayRef<Value*> Idx = ArrayRef<Value*>(ConstantInt::get(Int64Ty, i));
    auto* OffsetPtr = IRB.CreateGEP(ArrayTy, SrcPtr, Idx, "amx_zcol");
    emitAMX(AMXOpcode::LDZ, emitLoadStoreConfig(OffsetPtr, DestReg, IRB), IRB);
  }
}

void lowerStore(CallInst *CI, Instruction *InsertBefore, AMXOpcode Opcode) {
  auto *DestPtr = CI->getArgOperand(0);
  auto *SrcReg = CI->getArgOperand(1);

  assert(SrcReg->getType()->isIntegerTy(32));
  assert(isa<ConstantInt>(SrcReg));
  assert(DestPtr->getType()->isPointerTy());

  IRBuilder<> IRB(InsertBefore);

  emitAMX(Opcode, emitLoadStoreConfig(DestPtr, SrcReg, IRB), IRB);
}

// Lower stores of z columns. numCols must be 1, 2, 4, or 8.
void lowerStoreZCols(CallInst *CI, Instruction *InsertBefore, int numCols) {
  auto *DestPtr = CI->getArgOperand(0);
  auto *SrcCol = CI->getArgOperand(1);

  assert(SrcCol->getType()->isIntegerTy(32));
  assert(isa<ConstantInt>(SrcCol));
  assert(DestPtr->getType()->isPointerTy());

  IRBuilder<> IRB(InsertBefore);

  int numRegs = 8 * numCols;
  int stride = 8 / numCols;

  auto *Int64Ty = IRB.getInt64Ty();
  auto* ArrayTy = ArrayType::get(Int64Ty, 8);

  for (int i = 0; i < numRegs; i++) {
    auto *SrcReg = IRB.CreateAdd(SrcCol, ConstantInt::get(Int64Ty, stride * i));
    ArrayRef<Value*> Idx = ArrayRef<Value*>(ConstantInt::get(Int64Ty, i));
    auto* OffsetPtr = IRB.CreateGEP(ArrayTy, DestPtr, Idx, "amx_zcol");
    emitAMX(AMXOpcode::STZ, emitLoadStoreConfig(OffsetPtr, SrcReg, IRB), IRB);
  }
}

void lowerFloatOp(CallInst *CI, Instruction *InsertBefore, AMXOpcode Opcode, int ALUOp, int isVectorOp) {
  auto *XReg = CI->getArgOperand(1);
  auto *YReg = CI->getArgOperand(2);
  auto *ZReg = CI->getArgOperand(0);

  assert(XReg->getType()->isIntegerTy(32));
  assert(YReg->getType()->isIntegerTy(32));
  assert(ZReg->getType()->isIntegerTy(32));
  assert(isa<ConstantInt>(XReg));
  assert(isa<ConstantInt>(YReg));
  assert(isa<ConstantInt>(ZReg));

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

void lowerMoveToZ(CallInst *CI, Instruction *InsertBefore, AMXOpcode Opcode, int isSrcX) {
  auto *DstReg = CI->getArgOperand(0);
  auto *SrcReg = CI->getArgOperand(1);

  assert(DstReg->getType()->isIntegerTy(32));
  assert(SrcReg->getType()->isIntegerTy(32));
  assert(isa<ConstantInt>(DstReg));
  assert(isa<ConstantInt>(SrcReg));

  IRBuilder<> IRB(InsertBefore);
  auto *Int64Ty = IRB.getInt64Ty();

  emitAMX(Opcode, emitFMAConfig(
    isSrcX ? SrcReg : ConstantInt::get(Int64Ty, 0),
    isSrcX ? ConstantInt::get(Int64Ty, 0) : SrcReg,
    DstReg,
    isSrcX ? 3 : 5, // bit27-29
    1, // bit63
    IRB
  ), IRB);
}

void lowerExtr(CallInst *CI, Instruction *InsertBefore, AMXOpcode Opcode, int isDestY, int isSrcXY) {
  auto *DstReg = CI->getArgOperand(0);
  auto *SrcReg = CI->getArgOperand(1);

  assert(DstReg->getType()->isIntegerTy(32));
  assert(SrcReg->getType()->isIntegerTy(32));
  assert(isa<ConstantInt>(DstReg));
  assert(isa<ConstantInt>(SrcReg));

  IRBuilder<> IRB(InsertBefore);
  
  emitAMX(Opcode, emitExtrConfig(DstReg, SrcReg, isDestY, isSrcXY, IRB), IRB);
}

PreservedAnalyses llamx::AMXLowering::run(Function &F, FunctionAnalysisManager &AM) {
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

      // Simple loads and stores
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

      // Column loads and stores
      else if (Callee->getName() == "amx_ldz_col") {
        lowerLoadZCols(CI, &I, 1);
      } else if (Callee->getName() == "amx_ldz_col2") {
        lowerLoadZCols(CI, &I, 2);
      } else if (Callee->getName() == "amx_ldz_col4") {
        lowerLoadZCols(CI, &I, 4);
      } else if (Callee->getName() == "amx_ldz_col8") {
        lowerLoadZCols(CI, &I, 8);
      } else if (Callee->getName() == "amx_stz_col") {
        lowerStoreZCols(CI, &I, 1);
      } else if (Callee->getName() == "amx_stz_col2") {
        lowerStoreZCols(CI, &I, 2);
      } else if (Callee->getName() == "amx_stz_col4") {
        lowerStoreZCols(CI, &I, 4);
      } else if (Callee->getName() == "amx_stz_col8") {
        lowerStoreZCols(CI, &I, 8);
      } 
      
      // Moves
      else if (Callee->getName() == "amx_mvxy") {
        lowerExtr(CI, &I, AMXOpcode::EXTRY, 1 /*isDestY*/, 1 /*IsSrcXY*/);
      } else if (Callee->getName() == "amx_mvyx") {
        lowerExtr(CI, &I, AMXOpcode::EXTRX, 0 /*isDestY*/, 1 /*IsSrcXY*/);
      } else if (Callee->getName() == "amx_mvxz") {
        lowerMoveToZ(CI, &I, AMXOpcode::FMA64, 1 /*isSrcX*/);
      } else if (Callee->getName() == "amx_mvyz") {
        lowerMoveToZ(CI, &I, AMXOpcode::FMA64, 0 /*isSrcX*/);
      } else if (Callee->getName() == "amx_mvzx") {
        lowerExtr(CI, &I, AMXOpcode::EXTRX, 0 /*isDestY*/, 0 /*IsSrcXY*/);
      } else if (Callee->getName() == "amx_mvzy") {
        lowerExtr(CI, &I, AMXOpcode::EXTRX, 1 /*isDestY*/, 0 /*IsSrcXY*/);
      } 
      
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
