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

#include <vector>

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

/*  Linka il blocco src al blocco dst modificando l'operatore del branch incondizionato del terminatore
    del blocco src e facendolo puntare al blocco dst
    NB: questo metodo funziona solo nel caso in cui alla fine del blocco ci sia un branch terminator    */
void linkBlocks(BasicBlock* src, BasicBlock* dst, unsigned int idx){
    //outs() << "\n[ SRC ]" << *src;
    //outs() << "\n[ DST ]" << *dst;

    Instruction* terminator = src->getTerminator();
    //outs() << "[ TERMINATOR ]\n" << *terminator << "\n";
    
    BranchInst *branch = cast<BranchInst>(terminator);
    //outs() << "[ BRANCH ]\n" << *branch << "\n";
    
    branch->setSuccessor(idx, dst);
    //outs() << "[ BRANCH MODIFICATO ]\n" << *branch << "\n";
    //outs() << "[ SRC MODIFICATO ]" << *src;
}

/*  Trova tutti i blocchi che compongono il body del loop. Questa lista è ottenuta come differenza dell'insieme dei
    blocchi del loop con l'insieme formato da:
    - header
    - blocchi latch
    - blocchi exitings
    Refs: https://www.llvm.org/docs/LoopTerminology.html#terminology
*/
void getBodyBlocks(Loop *L, std::vector<BasicBlock*>& bodyBlocks){

    SmallVector<BasicBlock*> latches;
    L->getLoopLatches(latches);
    SmallVector<BasicBlock*> exitings;
    L->getExitingBlocks(exitings);

    for(auto BB : L->getBlocks()){
        if(BB == L->getHeader())
            continue;
        if(std::find(latches.begin(), latches.end(), BB) != latches.end())
            continue; 
        if(std::find(exitings.begin(), exitings.end(), BB) != exitings.end())
            continue; 
        bodyBlocks.push_back(BB);  
    }

}

/*  Sostituisce tutti gli usi dell'incrementatore del loop B (nextL) con l'incrementatore del loop A (L). Gli incrementatori
    si trovano nei phi node all'interno degli header dei loop e, quindi, appena viene trovato un phi si salva il valore */
void switchIncr(BasicBlock* headerL, BasicBlock* headerNextL){
    /*  Selezione riferimento all'incrementatore del loop A (L). Occorre prendere questo riferimento
        per sostituirilo a tutti gli usi dell'incrementatore del loop B (nextL) all'interno del body del loop
        B (nextL). */
    PHINode* incrL = NULL;
    for(auto instr = headerL->begin(); instr != headerL->end(); instr++){
        //outs() << "\n[ INCREMENTATORE di L ]\n" << *instr << "\n";
        if((incrL = dyn_cast<PHINode>(instr)))
            break;
    }

    /*  Selezione riferimento a incrementatore del loop B (nextL) (da sostituire con riferimento a 
        incrementatore del loop A (L) ) */
    PHINode* incrNextL = NULL;
    for(auto instr = headerNextL->begin(); instr != headerNextL->end(); instr++){
        //outs() << "\n[ INCREMENTATORE di nextL ]\n" << *instr << "\n";
        if((incrNextL = dyn_cast<PHINode>(instr)))
            break;
    }

    //sostituzione incrementatore del loop B (nextL)
    incrNextL->replaceAllUsesWith(incrL);
    
    /* [ DEBUG: SOSTITUZIONE INCREMENTATORE] */
    /*outs() << "\n[ DEBUG: SOSTITUZIONE INCREMENTATORE ]\n";
    outs() << "[ INCR. di L ] \n"<<*incrL << "\n";
    outs() << "\n[ INCR. di nextL ] \n"<<*incrNextL << "\n";*/
}

/*  Questa funzione si occupa di unire i due loop in questione, in ordine:
    1. sostituisce tutti gli usi dell'incrementatore del loop B (nextL) con l'incrementatore del loop A (L)
    2. sposta tutti i blocchi del body del loop B prima del latch del loop A (L)
    3. vari link dei blocchi per completare l'unione e isolare il vecchio CFG del loop B (nextL) per poter eseguire
       una dead code elimination successivamente */
void mergeLoops(Loop *L, Loop *nextL){
    
    /*  Salvataggio dei blocchi principali del loop B (nextL) perchè una volta modificata la sua struttura non
        saranno più accessibili.
        NB: questo passo funziona solo se i loop in question hanno un solo latch e un solo exit */
    BasicBlock* exitNextL = nextL->getExitBlock();
    BasicBlock* latchNextL = nextL->getLoopLatch();
    BasicBlock* headerNextL = nextL->getHeader();

    //selezione blocchi che formano il body dei due loop
    std::vector<BasicBlock*> bodyNextL;
    getBodyBlocks(nextL, bodyNextL);
    std::vector<BasicBlock*> bodyL;
    getBodyBlocks(L, bodyL);

    /* [ DEBUG: BLOCCHI PRINCIPALI LOOP nextL ] */
    /*outs() << "\n==========[ DEBUG: BLOCCHI PRINCIPALI nextL ]==========\n";
    outs() << "\n[ Header ]" << *headerNextL;
    outs() << "\n[ Body ]";
    for(auto BB : bodyNextL)
        outs() << *BB;
    outs() << "\n[ Latch ]" << *latchNextL;
    outs() << "\n[ Exit ]" << *exitNextL;
    outs() << "=========================================================\n";*/


    //sostituzione incrementatori
    switchIncr(L->getHeader(), headerNextL);

    /*  Spostamento di tutti i blocchi del body del loop B all'interno del loop A, immediatamente prima
        del blocco latch    */
    BasicBlock* latchL = L->getLoopLatch();
    for(auto BB : bodyNextL){
        BB->moveBefore(latchL);
    }

    /*  Linking dell'ultimo blocco del body del loop B (nextL) al latch del loop A (L)  */
    linkBlocks(bodyNextL.back(), latchL, 0);
    
    /*  Linking dell'ultimo blocco del body del loop A (L) al primo blocco del body del loop B (nextL) */
    linkBlocks(bodyL.back(), bodyNextL.front(), 0);

    /*  Linking dell'header del loop A (L) al blocco d'uscita del loop B (nextL) (nel ramo false)*/
    linkBlocks(L->getHeader(), exitNextL, 1);

    /*  Linking dell'header del loop B al latch del loop B (sia true che false). In questo modo il codice del loop
        B sarà completamente isolato e potrà essere eliminato con il passo "simplifycfg".*/
    linkBlocks(headerNextL, latchNextL, 0);
    linkBlocks(headerNextL, latchNextL, 1);

    /* [ DEBUG: PRINT LOOP L ] */
    /*outs() << "\n==========[ DEBUG: PRINT L ]==========\n";
    outs() << "\n[ Header ]" << *L->getHeader();
    outs() << "\n[ Body ]";
    for(auto BB : bodyL)    
        outs() << *BB;
    for(auto BB : bodyNextL)
        outs() << *BB;
    outs() << "\n[ Latch ]" << *L->getLoopLatch();
    outs() << "========================================\n";*/

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
    return PreservedAnalyses::none();
}
