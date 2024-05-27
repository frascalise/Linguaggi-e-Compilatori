#ifndef LLVM_TRANSFORMS_LOOPFUSION_H
#define LLVM_TRANSFORMS_LOOPFUSION_H


#include "llvm/IR/PassManager.h"
#include <llvm/IR/Constants.h>
#include "llvm/Analysis/ScalarEvolution.h"

 
namespace llvm {
    class LoopFusion : public PassInfoMixin<LoopFusion> {
    public:
        PreservedAnalyses run(Function &F, FunctionAnalysisManager &FM);
    };
} // namespace llvm
#endif // LLVM_TRANSFORMS_LOOPFUSION_H
