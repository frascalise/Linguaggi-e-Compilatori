#include "llvm/Transforms/Utils/LoopFusion.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/Transforms/Utils/LoopRotationUtils.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/Dominators.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/GenericCycleImpl.h"
#include "llvm/Analysis/LoopInfo.h"
#include <llvm/IR/Constants.h>

using namespace llvm;

bool guardedLoopCheck(Loop *loop, Loop *nextLoop) {
    /*  Nel preheader dovrebbe esserci il blocco di guardia
        Ricavo quindi il preheader, controllo che l'ultima istruzione sia un branch
        Se il branch è conditional (ovvero ha una condizione e in base a quella fa il salto)
        controllo che uno dei successori (per forza quello non loop) del branch sia il preheader del nextLoop
    */
    BasicBlock *preheader = loop->getLoopPreheader();
    if (!preheader) 
        return false;

    Instruction *term = preheader->getTerminator();
    if (!term || !isa<BranchInst>(term)) 
        return false;

    BranchInst *branch = cast<BranchInst>(term);
    if (branch->isUnconditional()) 
        return false;

    BasicBlock *trueSucc = branch->getSuccessor(0);
    BasicBlock *falseSucc = branch->getSuccessor(1);

    if (trueSucc == nextLoop->getHeader() || falseSucc == nextLoop->getHeader())
        return true;

    return false;
}

bool notGuardedLoopCheck(Loop *loop, Loop *nextLoop){
    SmallVector<BasicBlock*> exitBB;
    loop->getExitBlocks(exitBB);

    for(auto BB : exitBB){
        if(BB != nextLoop->getLoopPreheader())
            return false;
    }
    return true;
}

PreservedAnalyses LoopFusion::run(Function &F,FunctionAnalysisManager &AM){
    LoopInfo &LI = AM.getResult<LoopAnalysis>(F);
    DominatorTree &DT = AM.getResult<DominatorTreeAnalysis>(F);
    PostDominatorTree &PDT = AM.getResult<PostDominatorTreeAnalysis>(F);
    ScalarEvolution &SE =AM.getResult<ScalarEvolutionAnalysis>(F);
    for(auto L = LI.rbegin(); L != LI.rend(); L++){
        
        auto nextL = L;
        nextL++;

        if((*L)->isGuarded()){
            outs()<<"Loop guarded\n";
            if(nextL != LI.rend())
                if(guardedLoopCheck(*L, *(nextL))){
                    outs()<<"Loop adiacenti\n";
                    //controllo equivalenza (punto 3)
                    if (DT.dominates((*L)->getHeader(), (*nextL)->getHeader()) && PDT.dominates((*nextL)->getHeader(), (*L)->getHeader())) {
                        outs() << "I loop sono equivalenti per flusso di controllo\n";

                        //Controllo trip count (punto 2)
                        const SCEV *L0TripCount = SE.getBackedgeTakenCount(*L);
                        const SCEV *L1TripCount = SE.getBackedgeTakenCount(*nextL);
                        outs() << "Trip count del primo loop: ";
                        L0TripCount->print(outs());
                        outs() << "\nTrip count del secondo loop: ";
                        L1TripCount->print(outs());

                        if (L0TripCount == L1TripCount) {
                            outs() << "I loop iterano lo stesso numero di volte\n";
                        } else {
                            outs() << "I loop non iterano lo stesso numero di volte\n";
                        }
                    } else {
                        outs() << "I loop non sono equivalenti per flusso di controllo\n";
                    }
                }
        }
        else{
            outs()<<"Loop not guarded\n";
            if(nextL != LI.rend())
                if(notGuardedLoopCheck(*L, *(nextL))){
                    outs()<<"Loop adiacenti\n";
                    //controllo equivalenza (punto 3)
                    if (DT.dominates((*L)->getHeader(), (*nextL)->getHeader()) && PDT.dominates((*nextL)->getHeader(), (*L)->getHeader())) {
                        outs() << "I loop sono equivalenti per flusso di controllo\n";
                        //Controllo trip count (punto 2)
                        const SCEV *L0TripCount = SE.getBackedgeTakenCount(*L);
                        const SCEV *L1TripCount = SE.getBackedgeTakenCount(*nextL);
                        outs() << "Trip count del primo loop: ";
                        L0TripCount->print(outs());
                        outs() << "\nTrip count del secondo loop: ";
                        L1TripCount->print(outs());

                        if (L0TripCount == L1TripCount) {
                            outs() << "I loop iterano lo stesso numero di volte\n";
                        } else {
                            outs() << "I loop non iterano lo stesso numero di volte\n";
                        }
                    } else {
                        outs() << "I loop non sono equivalenti per flusso di controllo\n";
                    }
                }

        }
        
        outs()<<"PREHEADER: "<<*((*L)->getLoopPreheader())<<"\n";
        outs()<<"HEADER: "<<*((*L)->getHeader())<<"\n";
        outs()<<"CONTENUTO\n";
        for(auto i:(*L)->getBlocks()){
            outs()<<*i;
        }
        outs()<<"EXITING BLOCK: "<<*((*L)->getExitingBlock())<<"\n";
        outs()<<"EXIT BLOCK: "<<*((*L)->getExitBlock())<<"\n";
        
        //outs()<<"successore di exiting block"<<(*L)->getSingleSuccessor()<<'\n';
    }
    return PreservedAnalyses::all();
}
