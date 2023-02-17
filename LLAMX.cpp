#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace {
struct AMXLowering : public PassInfoMixin<AMXLowering> {
  PreservedAnalyses run(Function &, FunctionAnalysisManager &);
};
}

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

PreservedAnalyses AMXLowering::run(Function &F, FunctionAnalysisManager &AM) {
  errs() << "!!! processing " << F.getName() << '\n';
  return PreservedAnalyses::none();
}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "llamx", LLVM_VERSION_STRING, buildPasses};
}
