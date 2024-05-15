#include "llvm/Transforms/Utils/LoopWalk.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/PassManager.h"
#include <llvm/IR/Constants.h>

#include <vector>
#include <algorithm>

using namespace llvm;

/*
    Verifica esattamente le 3 condizioni per la loop invariant:
    1) operando costante
    2) reaching definition esterna al loop (o argomento della funzione)
    3) reaching definition interna al loop ma loop invariant a sua volta
*/
bool isOperandInvariant(Use* operand, Loop &L, std::vector<Instruction*> loopinv){

    if(dyn_cast<Argument>(operand)){
        outs()<<"> Argument\n";
        return true;
    }
        
    if(dyn_cast<Constant>(operand)){
        outs()<<"> Constant\n";
        return true;
    }

    if(dyn_cast<BinaryOperator>(operand)){
        Instruction* reach_def = dyn_cast<Instruction>(operand);
        if(L.contains(reach_def)){
            if(std::find(loopinv.begin(), loopinv.end(), reach_def) == loopinv.end()){
                return false;
            }
            else{
                outs()<<"> LI Dependent\n";
                return true;
            }
        }

        outs()<<"> Out of loop\n";
        return true;
    }

    return false;
}


bool isLoopInvariant(Instruction &instr, Loop &L, std::vector<Instruction*> loopinv){

    for(auto op = instr.op_begin(); op != instr.op_end(); op++){
        if(!isOperandInvariant(op, L, loopinv))
            return false;
    }
    
    return true;
}

PreservedAnalyses LoopWalk::run(Loop &L,
    LoopAnalysisManager &LAM,
    LoopStandardAnalysisResults &LAR,
    LPMUpdater &LU){

    auto BB_list = L.getBlocks();
    std::vector<Instruction*> loopinv;

        
    for(auto BB : BB_list){
        outs()<<"\nBasicBlock found\n";
        for(auto I = BB->begin(); I != BB->end(); I++){
            if(dyn_cast<BinaryOperator>(I)){
                outs()<<"[Analyzing]\t "<<*I<<"\n";
                if(isLoopInvariant(*I, L, loopinv)){
                    outs()<<"[Loop invariant]\n";
                    loopinv.push_back(dyn_cast<Instruction>(I));
                }
                else{
                    outs()<<"[Not loop invariant]\n";
                }
            }
            else{
                outs()<<"[Skipped]\t "<<*I<<"\n";
            }
            outs()<<"\n";
        }
    }

    return PreservedAnalyses::all();
}

