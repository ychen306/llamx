#ifndef REGALLOC_H
#define REGALLOC_H

#include "llvm/IR/IRBuilder.h"
#include "llvm/Passes/PassBuilder.h"

using namespace llvm;

namespace llamx {
  struct AMXLowLevelRegAlloc : public llvm::PassInfoMixin<AMXLowLevelRegAlloc> {
    llvm::PreservedAnalyses run(llvm::Function &, llvm::FunctionAnalysisManager &);
  };
}

enum RegType {
  X, Y, Z, ZI, Z_COL, Z_COL2, Z_COL4, Z_COL8
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

  static OperandInfo getUseDef(RegType Ty) {
    return {
      .IsUsed = true,
      .IsDefined = true,
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
  bool operator==(const RegInfo &Other) const {
    return Ty == Other.Ty && Number == Other.Number;
  }
  unsigned size() {
    switch (Ty) {
      case X:
      case Y:
      case Z:
      case ZI: return 64;
      case Z_COL:  return 64 * 8;
      case Z_COL2: return 64 * 16;
      case Z_COL4: return 64 * 32;
      case Z_COL8: return 64 * 64;
    }
  }
};

class RegisterSpiller {
  Function *F;
  // Mapping <virtual reg> -> <alloca>
  std::map<RegInfo, AllocaInst *> RegToMemMap;
public:
  RegisterSpiller(Function *F) : F(F) {}
  bool isSpilled(RegInfo Virt) const;
  void allocateMemory(RegInfo Virt);
  void store(RegInfo Phys, RegInfo Virt, IRBuilderBase &IRB);
  void reload(RegInfo Phys, RegInfo Virt, IRBuilderBase &IRB);
};

#endif // REGALLOC_H