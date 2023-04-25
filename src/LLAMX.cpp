#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"

#include "LLAMX.h"
#include "Lowering.h"
#include "RegAlloc.h"

using namespace llvm;

static void buildPasses(PassBuilder &PB) {
  PB.registerPipelineParsingCallback(
      [](StringRef Name, FunctionPassManager &FPM,
         ArrayRef<PassBuilder::PipelineElement>) {
        if (Name == "low-level-regalloc") {
          FPM.addPass(llamx::AMXLowLevelRegAlloc());
          return true;
        }
        if (Name == "lower-amx") {
          FPM.addPass(llamx::AMXLowering());
          return true;
        }
        return false;
      });

  PB.registerScalarOptimizerLateEPCallback(
      [](FunctionPassManager &FPM, OptimizationLevel) {
        FPM.addPass(llamx::AMXLowLevelRegAlloc());
        FPM.addPass(llamx::AMXLowering());
      });
}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "llamx", LLVM_VERSION_STRING, buildPasses};
}
