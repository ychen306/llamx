#ifndef LOWERING_H
#define LOWERING_H

#include "llvm/IR/IRBuilder.h"
#include "llvm/Passes/PassBuilder.h"

using namespace llvm;
namespace llamx {
  struct AMXLowering : public llvm::PassInfoMixin<AMXLowering> {
    llvm::PreservedAnalyses run(llvm::Function &, llvm::FunctionAnalysisManager &);
  };
}

Value *emitConfig(std::list<std::pair<Value*, int>> inputs, IRBuilderBase &IRB);

Value *emitLoadStoreConfig(Value *Ptr, Value *Reg, IRBuilderBase &IRB);
Value *emitFMAConfig(Value *XReg, Value *YReg, Value *ZReg, int OpType, int Mode, IRBuilderBase &IRB);
Value *emitExtrConfig(Value *DstReg, Value *SrcReg, int isDestY, int isSrcXY, IRBuilderBase &IRB);
Value *emitExtrhConfig(Value *DstReg, Value *SrcReg, int isDestY, int isSrcXY, IRBuilderBase &IRB);
void emitAMX(AMXOpcode Opcode, Value *Operand, IRBuilderBase &IRB);

void lowerLoad(CallInst *CI, Instruction *InsertBefore, AMXOpcode Opcode);
void lowerLoadZCols(CallInst *CI, Instruction *InsertBefore, int numCols);
void lowerStore(CallInst *CI, Instruction *InsertBefore, AMXOpcode Opcode);
void lowerStoreZCols(CallInst *CI, Instruction *InsertBefore, int numCols);
void lowerFloatOp(CallInst *CI, Instruction *InsertBefore, AMXOpcode Opcode, int ALUOp, int isVectorOp);
void lowerMoveToZ(CallInst *CI, Instruction *InsertBefore, AMXOpcode Opcode, int isSrcX);
void lowerExtr(CallInst *CI, Instruction *InsertBefore, AMXOpcode Opcode, int isDestY, int isSrcXY);


#endif // LOWERING_H