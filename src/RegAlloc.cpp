#include "llvm/IR/IRBuilder.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/InstIterator.h"
#include <map>

#include "LLAMX.h"
#include "Lowering.h"
#include "RegAlloc.h"

using namespace llvm;

bool RegisterSpiller::isSpilled(RegInfo Virt) const {
  return RegToMemMap.count(Virt);
}

void RegisterSpiller::allocateMemory(RegInfo Virt) {
  assert(!RegToMemMap.count(Virt));
  BasicBlock &Entry = F->getEntryBlock();
  auto &Ctx = F->getContext();
  auto *Int8Ty = Type::getInt8Ty(Ctx);
  auto *Int64Ty = Type::getInt64Ty(Ctx);
  auto *Alloca = new AllocaInst(Int8Ty, 0, ConstantInt::get(Int64Ty, Virt.size()), "amx.spilled", Entry.getFirstNonPHI());
  RegToMemMap[Virt] = Alloca;
}

void RegisterSpiller::store(RegInfo Phys, RegInfo Virt, IRBuilderBase &IRB) {
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
  if (Ty == RegType::ZI) {
    emitAMX(AMXOpcode::STZI, emitLoadStoreConfig(Ptr, ConstantInt::get(Int64Ty, Phys.Number), IRB), IRB);
    return;
  }
  if (Ty == RegType::Z_COL) {
    for (int i = 0; i < 8; i++) {
      auto* ArrayTy = ArrayType::get(Int64Ty, 8);
      ArrayRef<Value*> Idx = ArrayRef<Value*>(ConstantInt::get(Int64Ty, i));
      auto* OffsetPtr = IRB.CreateGEP(ArrayTy, Ptr, Idx, "amx_zcol");
      emitAMX(AMXOpcode::STZ, emitLoadStoreConfig(OffsetPtr, ConstantInt::get(Int64Ty, Phys.Number + 8 * i), IRB), IRB);
    }
    return;
  }
  if (Ty == RegType::Z_COL2) {
    for (int i = 0; i < 16; i++) {
      auto* ArrayTy = ArrayType::get(Int64Ty, 8);
      ArrayRef<Value*> Idx = ArrayRef<Value*>(ConstantInt::get(Int64Ty, i));
      auto* OffsetPtr = IRB.CreateGEP(ArrayTy, Ptr, Idx, "amx_zcol");
      emitAMX(AMXOpcode::STZ, emitLoadStoreConfig(OffsetPtr, ConstantInt::get(Int64Ty, Phys.Number + 4 * i), IRB), IRB);
    }
    return;
  }
  if (Ty == RegType::Z_COL4) {
    for (int i = 0; i < 32; i++) {
      auto* ArrayTy = ArrayType::get(Int64Ty, 8);
      ArrayRef<Value*> Idx = ArrayRef<Value*>(ConstantInt::get(Int64Ty, i));
      auto* OffsetPtr = IRB.CreateGEP(ArrayTy, Ptr, Idx, "amx_zcol");
      emitAMX(AMXOpcode::STZ, emitLoadStoreConfig(OffsetPtr, ConstantInt::get(Int64Ty, Phys.Number + 2 * i), IRB), IRB);
    }
    return;
  }
  if (Ty == RegType::Z_COL8) {
    for (int i = 0; i < 64; i++) {
      auto* ArrayTy = ArrayType::get(Int64Ty, 8);
      ArrayRef<Value*> Idx = ArrayRef<Value*>(ConstantInt::get(Int64Ty, i));
      auto* OffsetPtr = IRB.CreateGEP(ArrayTy, Ptr, Idx, "amx_zcol");
      emitAMX(AMXOpcode::STZ, emitLoadStoreConfig(OffsetPtr, ConstantInt::get(Int64Ty, Phys.Number + i), IRB), IRB);
    }
    return;
  }
  llvm_unreachable("don't know how to store this register type");
}

// Reload a the virtual register onto the specified physical reg.
void RegisterSpiller::reload(RegInfo Phys, RegInfo Virt, IRBuilderBase &IRB) {
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
  if (Ty == RegType::ZI) {
    emitAMX(AMXOpcode::LDZI, emitLoadStoreConfig(Ptr, ConstantInt::get(Int64Ty, Phys.Number), IRB), IRB);
    return;
  }
  if (Ty == RegType::Z_COL) {
    for (int i = 0; i < 8; i++) {
      auto* ArrayTy = ArrayType::get(Int64Ty, 8);
      ArrayRef<Value*> Idx = ArrayRef<Value*>(ConstantInt::get(Int64Ty, i));
      auto* OffsetPtr = IRB.CreateGEP(ArrayTy, Ptr, Idx, "amx_zcol");
      emitAMX(AMXOpcode::LDZ, emitLoadStoreConfig(OffsetPtr, ConstantInt::get(Int64Ty, Phys.Number + 8 * i), IRB), IRB);
    }
    return;
  }
  if (Ty == RegType::Z_COL2) {
    for (int i = 0; i < 16; i++) {
      auto* ArrayTy = ArrayType::get(Int64Ty, 8);
      ArrayRef<Value*> Idx = ArrayRef<Value*>(ConstantInt::get(Int64Ty, i));
      auto* OffsetPtr = IRB.CreateGEP(ArrayTy, Ptr, Idx, "amx_zcol");
      emitAMX(AMXOpcode::LDZ, emitLoadStoreConfig(OffsetPtr, ConstantInt::get(Int64Ty, Phys.Number + 4 * i), IRB), IRB);
    }
    return;
  }
  if (Ty == RegType::Z_COL4) {
    for (int i = 0; i < 32; i++) {
      auto* ArrayTy = ArrayType::get(Int64Ty, 8);
      ArrayRef<Value*> Idx = ArrayRef<Value*>(ConstantInt::get(Int64Ty, i));
      auto* OffsetPtr = IRB.CreateGEP(ArrayTy, Ptr, Idx, "amx_zcol");
      emitAMX(AMXOpcode::LDZ, emitLoadStoreConfig(OffsetPtr, ConstantInt::get(Int64Ty, Phys.Number + 2 * i), IRB), IRB);
    }
    return;
  }
  if (Ty == RegType::Z_COL8) {
    for (int i = 0; i < 64; i++) {
      auto* ArrayTy = ArrayType::get(Int64Ty, 8);
      ArrayRef<Value*> Idx = ArrayRef<Value*>(ConstantInt::get(Int64Ty, i));
      auto* OffsetPtr = IRB.CreateGEP(ArrayTy, Ptr, Idx, "amx_zcol");
      emitAMX(AMXOpcode::LDZ, emitLoadStoreConfig(OffsetPtr, ConstantInt::get(Int64Ty, Phys.Number + i), IRB), IRB);
    }
    return;
  }
  llvm_unreachable("don't know how to store this register type");
};

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
    return {OperandInfo::getDef(RegType::ZI), None};
  if (Callee->getName() == "amx_stzi")
    return {None, OperandInfo::getUse(RegType::ZI)};
  
  if (Callee->getName() == "amx_ldz_col")
    return {OperandInfo::getDef(RegType::Z_COL), None};
  if (Callee->getName() == "amx_ldz_col2")
    return {OperandInfo::getDef(RegType::Z_COL2), None};
  if (Callee->getName() == "amx_ldz_col4")
    return {OperandInfo::getDef(RegType::Z_COL4), None};
  if (Callee->getName() == "amx_ldz_col8")
    return {OperandInfo::getDef(RegType::Z_COL8), None};
  if (Callee->getName() == "amx_stz_col")
    return {None, OperandInfo::getUse(RegType::Z_COL)};
  if (Callee->getName() == "amx_stz_col2")
    return {None, OperandInfo::getUse(RegType::Z_COL2)};
  if (Callee->getName() == "amx_stz_col4")
    return {None, OperandInfo::getUse(RegType::Z_COL4)};
  if (Callee->getName() == "amx_stz_col8")
    return {None, OperandInfo::getUse(RegType::Z_COL8)};

  if (Callee->getName() == "amx_mvxy")
    return {OperandInfo::getDef(RegType::Y), OperandInfo::getUse(RegType::X)};
  if (Callee->getName() == "amx_mvyx")
    return {OperandInfo::getDef(RegType::X), OperandInfo::getUse(RegType::Y)};
  if (Callee->getName() == "amx_mvxz")
    return {OperandInfo::getDef(RegType::Z), OperandInfo::getUse(RegType::X)};
  if (Callee->getName() == "amx_mvyz")
    return {OperandInfo::getDef(RegType::Z), OperandInfo::getUse(RegType::Y)};
  if (Callee->getName() == "amx_mvzx")
    return {OperandInfo::getDef(RegType::X), OperandInfo::getUse(RegType::Z)};
  if (Callee->getName() == "amx_mvzy")
    return {OperandInfo::getDef(RegType::Y), OperandInfo::getUse(RegType::Z)};
    
  if (Callee->getName() == "amx_fma64_mat")
    return {OperandInfo::getUseDef(RegType::Z_COL), OperandInfo::getUse(RegType::X), OperandInfo::getUse(RegType::Y)}; // TODO fix
  if (Callee->getName() == "amx_fma32_mat")
    return {OperandInfo::getUseDef(RegType::Z_COL2), OperandInfo::getUse(RegType::X), OperandInfo::getUse(RegType::Y)};
  if (Callee->getName() == "amx_fma16_mat")
    return {OperandInfo::getUseDef(RegType::Z_COL4), OperandInfo::getUse(RegType::X), OperandInfo::getUse(RegType::Y)};
  if (Callee->getName() == "amx_fms64_mat")
    return {OperandInfo::getUseDef(RegType::Z_COL), OperandInfo::getUse(RegType::X), OperandInfo::getUse(RegType::Y)};
  if (Callee->getName() == "amx_fms32_mat")
    return {OperandInfo::getUseDef(RegType::Z_COL2), OperandInfo::getUse(RegType::X), OperandInfo::getUse(RegType::Y)};
  if (Callee->getName() == "amx_fms16_mat")
    return {OperandInfo::getUseDef(RegType::Z_COL4), OperandInfo::getUse(RegType::X), OperandInfo::getUse(RegType::Y)};
  if (Callee->getName() == "amx_fma64_vec")
    return {OperandInfo::getUseDef(RegType::Z), OperandInfo::getUse(RegType::X), OperandInfo::getUse(RegType::Y)};
  if (Callee->getName() == "amx_fma32_vec")
    return {OperandInfo::getUseDef(RegType::Z), OperandInfo::getUse(RegType::X), OperandInfo::getUse(RegType::Y)};
  if (Callee->getName() == "amx_fma16_vec")
    return {OperandInfo::getUseDef(RegType::Z), OperandInfo::getUse(RegType::X), OperandInfo::getUse(RegType::Y)};
  if (Callee->getName() == "amx_fms64_vec")
    return {OperandInfo::getUseDef(RegType::Z), OperandInfo::getUse(RegType::X), OperandInfo::getUse(RegType::Y)};
  if (Callee->getName() == "amx_fms32_vec")
    return {OperandInfo::getUseDef(RegType::Z), OperandInfo::getUse(RegType::X), OperandInfo::getUse(RegType::Y)};
  if (Callee->getName() == "amx_fms16_vec")
    return {OperandInfo::getUseDef(RegType::Z), OperandInfo::getUse(RegType::X), OperandInfo::getUse(RegType::Y)};
  return {};
}

void setRegs(CallInst *CI, ArrayRef<Optional<OperandInfo>> RegInfos, ArrayRef<unsigned> PhysRegs) {
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

PreservedAnalyses llamx::AMXLowLevelRegAlloc::run(Function &F, FunctionAnalysisManager &AM) {
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