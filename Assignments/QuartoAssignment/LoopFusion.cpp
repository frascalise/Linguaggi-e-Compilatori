#include "llvm/Transforms/Utils/LFusion.h"
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

/*  Ritorna true se i due loop guarded sono adiacenti, ovvero se tra i due loop
    non sono presenti istruzioni. In caso contrario ritorna false */
bool guardedAdjacentLoopCheck(Loop *loop, Loop *nextLoop) {
    /*  Nel preheader dovrebbe esserci il blocco di guardia
        Ricavo quindi il preheader, controllo che l'ultima istruzione sia un branch
        Se il branch è conditional (ovvero ha una condizione e in base a quella fa il salto)
        controllo che uno dei successori (per forza quello non loop) del branch sia il preheader del nextLoop
    */
    BasicBlock *preheader = loop->getLoopPreheader();
    if (!preheader) 
        return false;

    Instruction *term = preheader->getTerminator();

    // Se non è un branch allora esci
    if (!term || !isa<BranchInst>(term)) 
        return false;

    BranchInst *branch = cast<BranchInst>(term);

    // Un branch è incondizionale se non ha condizioni e salta sempre (es. while true)
    if (branch->isUnconditional()) 
        return false;

    // Prendo le condizioni del branch in caso di true e false
    BasicBlock *trueSucc = branch->getSuccessor(0);
    BasicBlock *falseSucc = branch->getSuccessor(1);

    if (trueSucc == nextLoop->getHeader() || falseSucc == nextLoop->getHeader())
        return true;

    return false;
}

/*  Ritorna true se i due loop not guarded sono adiacenti, ovvero se tra i due loop
    non sono presenti istruzioni. In caso contrario ritorna false */
bool notGuardedAdjacentLoopCheck(Loop *loop, Loop *nextLoop){
    SmallVector<BasicBlock*> exitBB;
    // Prendo tutti i blocchi di uscita del loop
    loop->getExitBlocks(exitBB);

    // Scorro i BasicBlock di uscita del loop
    for(auto BB : exitBB){
        // Se il BB è diverso dal preheader del prossimo loop allora ritorno false
        if(BB != nextLoop->getLoopPreheader())
            return false;
    }
    // Se tutti i BB di uscita del loop sono uguali al preheader del prossimo loop allora ritorno true
    return true;
}

/*  Controllo che il numero di iterazioni dei due loop sia lo stesso
    Ritorna true se i due loop iterano lo stesso numero di volte, altrimenti false */
bool checkTripCount(Loop *L, Loop *nextL, ScalarEvolution &SE){
    // Prendo il valore del numero di iterazioni che hanno i due loop
    const SCEV *L0TripCount = SE.getBackedgeTakenCount(L);
    const SCEV *L1TripCount = SE.getBackedgeTakenCount(nextL);

    outs() << "Trip count del primo loop: ";
    L0TripCount->print(outs());
    outs() << "\nTrip count del secondo loop: ";
    L1TripCount->print(outs());

    if (L0TripCount == L1TripCount) {
        outs() << "\tI loop iterano lo stesso numero di volte\n";
        return true;
    } else {
        outs() << "\tI loop non iterano lo stesso numero di volte\n";
        return false;
    }
}

/*  Per controllare la dominanza tra due loop devo controllare che l'header del primo loop dominini il secondo loop
    DT controlla la dominanza tra L0 (L) e L1 (nextL), ovvero che ogni percorso che arriva ad L1 passa per L0
    PDT controlla la post dominanza tra L1 (nextL) e L0 (L), ovvero che se L1 viene eseguito vuol dire che L0 è già stato eseguito */
bool checkLoopEquivalence(DominatorTree &DT, Loop *L, Loop *nextL, PostDominatorTree &PDT){
    return DT.dominates(L->getHeader(), nextL->getHeader()) && PDT.dominates(nextL->getHeader(), L->getHeader());
}

/*  TODO:
    1. prendere body di nextL - OK
    2. muovere body di nextL prima di latch di L
    3. body di nextL deve puntare al latch di L
    4. l'header di L deve puntare alla exit di nextL
*/
void mergeLoops(Loop *L, Loop *nextL){
    SmallVector<BasicBlock*> bodyNextL;
    SmallVector<BasicBlock*> bodyL;
    // Prendo tutti i blocchi del body del loop successivo
    for(auto BB : nextL->getBlocks()){
        if(BB != nextL->getLoopPreheader() && BB != nextL->getHeader() && BB != nextL->getLoopLatch())
            bodyNextL.push_back(BB);
    }
    auto exitingNextL=nextL->getExitingBlock();
    BasicBlock* latchL = L->getLoopLatch();
    for(auto BB : bodyNextL){
        //sposto il body di nextL prima del latch di L
        BB->moveBefore(latchL);
        //faccio puntare il branch del body di nextL al latch di L
        auto terminator = BB->getTerminator();
        BranchInst *branch = cast<BranchInst>(terminator);
        branch->setSuccessor(0,L->getLoopLatch());

    }
    
    for(auto BB : L->getBlocks()){
        if(BB != L->getLoopPreheader() && BB != L->getHeader() && BB != L->getLoopLatch())
            bodyL.push_back(BB);
    }
    

    
    for(auto BB : bodyL){
        //il branch della fine del body di L deve puntare al body di nextL, oppure essere eliminato (errore)
        auto terminator = BB->getTerminator();
        BranchInst *branch = cast<BranchInst>(terminator);
        outs()<<"successor "<<*(branch->getSuccessor(0))<<"\n";
        //istruzione che dà errore
        branch->setSuccessor(0,bodyNextL[0]);
        

        //outs()<< "\n[DEBUG] BB->getTerminator()" << *(BB->getTerminator()) << "\n";
    }
    //prendo l'etichetta di uscita da nextL
    auto terminatorNextL=exitingNextL->getTerminator();
    BranchInst *branchNextL=cast<BranchInst>(terminatorNextL);
    auto successorNextL=branchNextL->getSuccessor(1);
    //modifico l'uscita dall'header di L con quella di nextL
    auto exitingL=L->getExitingBlock();
    auto terminatorL=exitingL->getTerminator();
    BranchInst *branchL=cast<BranchInst>(terminatorL);
    branchL->setSuccessor(1,successorNextL);



    //outs()<<*(nextL->getLoopLatch()-1);
    //Instruction *terminatoreBodyL = (L->getLoopLatch()-1)->getTerminator();
    //outs()<<"Terminatore: "<<terminatoreBodyL<<"\n";


    /*
    outs() << "\n[DEBUG] Loop L: \n";
    for(auto BB : L->getBlocks()){
        outs()<<*BB<<"\n";
    }

    outs() << "\n[DEBUG] New L: \n";
    for(auto BB : L->getBlocks()){
        outs()<<*BB<<"\n";
    }
    outs() << "\n[DEBUG] New  nextL: \n";
    for(auto BB : nextL->getBlocks()){
        outs()<<*BB<<"\n";
    }
    */
}


PreservedAnalyses LFusion::run(Function &F,FunctionAnalysisManager &AM){
    LoopInfo &LI = AM.getResult<LoopAnalysis>(F);
    DominatorTree &DT = AM.getResult<DominatorTreeAnalysis>(F);
    PostDominatorTree &PDT = AM.getResult<PostDominatorTreeAnalysis>(F);
    ScalarEvolution &SE =AM.getResult<ScalarEvolutionAnalysis>(F);
    
    // Scorro i loop in ordine rovesciato, quindi parto dal primo anzichè partire dall'ultimo
    for(auto L = LI.rbegin(); L != LI.rend(); L++){
        
        // nextL è il loop successivo a L
        auto nextL = L;
        nextL++;

        // Controllo se il loop che stiamo scorrendo è l'ultimo
        if(nextL == LI.rend()){
            continue;
        }
        
        /*
            - Loop GUARDED: se il loop è protetto da una guardia, ovvero se il loop è preceduto da un branch (while x < 5)
            - Loop NOT GUARDED: se il loop non è protetto da una guardia (while true)
        */
        if((*L)->isGuarded()) {
            outs()<<"Loop guarded\n";
            if(!guardedAdjacentLoopCheck(*L, *(nextL)))
                continue;
        } else {
            outs()<<"Loop not guarded\n";
            if(!notGuardedAdjacentLoopCheck(*L, *(nextL)))
                continue;
        }

        // Controllo equivalenza (punto 3)
        // Controllo per Dominanza e Post-Dominanza
        if (!checkLoopEquivalence(DT, *L, *nextL, PDT)) {
            continue;
        }

        // Controllo trip count (punto 2)
        // Controllo che il numero di iterazioni dei due loop sia lo stesso
        if(!checkTripCount(*L, *nextL, SE)){
            continue;
        }

        mergeLoops(*L, *nextL);
    
    }
    return PreservedAnalyses::all();
}
