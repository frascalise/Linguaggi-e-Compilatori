#include "llvm/Transforms/Utils/LoopWalk.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/Transforms/Utils/LoopRotationUtils.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/Dominators.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/GenericCycleImpl.h"
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
bool isOperandInvariant(Use* operand, Loop &L, std::vector<Instruction*> candidates){

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
            if(std::find(candidates.begin(), candidates.end(), reach_def) == candidates.end()){
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

/*  Si occupa solo di invocare su ogni operando il controllo per la loopinvariant    */
bool isLoopInvariant(Instruction &instr, Loop &L, std::vector<Instruction*> candidates){

    for(auto op = instr.op_begin(); op != instr.op_end(); op++){
        if(!isOperandInvariant(op, L, candidates))
            return false;
    }
    
    return true;
}

//  https://llvm.org/docs/LoopTerminology.html#loop-closed-ssa-lcssa
/*  I PHInode LCSSA vengono inseriti automaticamente nei blocchi di uscita del loop e servono a specificare quali variabili sono
    "vive" fuori dal loop. I PHInode di questo tipo hanno un solo operando e non ci interessano per i controlli della code motion */
bool isLCSSA(PHINode* node){
    if(PHINode* phi = dyn_cast<PHINode>(node)){
        if(phi->getNumOperands() == 1)
            return true;
    }
    return false;
}

/*  Verifica che il blocco di appartentenza dell'istruzione domini tutti i blocchi di uscita del loop. 
    NOTA: In realtà exitingBB in questo caso corrisponde al blocco if.else, non so esattamente il motivo ma il risultato è lo stesso */
bool dominatesAllExits(Instruction* instr, SmallVector<BasicBlock*> exitingBB, DominatorTree &DT){
    BasicBlock* instrBB = instr->getParent();
        
    for(auto exitBB: exitingBB){
        if(!DT.dominates(instrBB, exitBB)){
            return false;
        }
    }

    return true;
}

/*  Verifica che la variabile definita con questa istruzione non abbia, in realtà, altre definizioni in altre parti del loop.
    Questo controllo viene effettuato verificando che nessun user della variabile corrisponda ad un PHI node.   
    Se per esempio una variabile è data da due argomenti che soddisfano la LoopInvarianza, ma almeno uno di quest definiti in un if, 
    non si può spostare fuori dal loop. */
bool multipleDefinitions(Instruction* instr){
    for(auto user = instr->user_begin(); user != instr->user_end(); user++){
        PHINode* phi;
        if((phi = dyn_cast<PHINode>(*user))){
            if(!isLCSSA(phi)){
                return false;
            }   
        }    
    }

    return true;
}

/*  Verifica che l'istruzione domini tutti i suoi utilizzi all'interno dei blocchi del loop.
    Questo controllo è necessario per evitare che l'istruzione venga spostata prima di un suo utilizzo. */
bool dominatesAllUses(Instruction* instr, DominatorTree &DT){
    for(auto use = instr->use_begin(); use != instr->use_end(); use++){
        //outs()<<"[Use]\t"<<*((use)->get())<<"\n";
        if(!DT.dominates(instr, *use)){
            return false;
        }
    }

    return true;
}

/*  Verifica che l'istruzione non abbia usi al di fuori del loop, in tal caso può essere spostata nonostante il fatto
    che non domini tutte le uscite  */
bool isDeadAfterLoop(Instruction* instr, SmallVector<BasicBlock*> successorBB){
    for(auto user = instr->user_begin(); user != instr->user_end(); user++){
        BasicBlock* userBB = dyn_cast<Instruction>(*user)->getParent();
        if(std::find(successorBB.begin(), successorBB.end(), userBB) != successorBB.end()){
            return false;
        }
    }
    return true;
}

PreservedAnalyses LoopWalk::run(Loop &L,
    LoopAnalysisManager &LAM,
    LoopStandardAnalysisResults &LAR,
    LPMUpdater &LU){

    auto BB_list = L.getBlocks();
    std::vector<Instruction*> candidates;
    
    outs()<<"----- CHECKING LOOP INVARIANT CONDITIONS -----\n";
    /*  Calcolo delle istruzioni loop invariant secondo le condizioni di slide 13, il risultato sarà
        un vector di istruzioni candidate alla code motion  */   
    for(auto BB : BB_list){
        outs()<<"BasicBlock found\n";
        for(auto I = BB->begin(); I != BB->end(); I++){
            if(dyn_cast<BinaryOperator>(I)){
                outs()<<"[Analyzing]\t "<<*I<<"\n";
                if(isLoopInvariant(*I, L, candidates)){
                    outs()<<"[Loop invariant]\n";
                    candidates.push_back(dyn_cast<Instruction>(I));
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
        outs()<<"----------------------------------------------------------------\n";
    }

    for(auto instr : candidates){
        outs()<<"[Candidate] "<<*instr<<"\n";
    }

    /*  Strutture dati necessarie all'analisi delle condizioni per la code motion   */
    DominatorTree &DT = LAR.DT;
    SmallVector<BasicBlock*> exitingBB;
    L.getExitingBlocks(exitingBB);
    SmallVector<BasicBlock*> successorBB;
    L.getExitBlocks(successorBB);
    

    outs()<<"\n----- CHECKING CODE MOTION CONDITIONS -----\n";

    /*  Ciclo dedicato all'analisi delle condizioni per la code motion:
        • Sono loop invariant (già verificata con lo step precedente)
        • Si trovano in blocchi che dominano tutte le uscite del loop o sono dead all'uscita del loop
        • Assegnano un valore a variabili non assegnate altrove nel loop (condizione già soddisfatta in SSA)
        • Si trovano in blocchi che dominano tutti i blocchi nel loop che usano la
        variabile a cui si sta assegnando un valore
    */
    for(long unsigned int i = 0; i < candidates.size(); i++){
        if(!dominatesAllExits(candidates[i], exitingBB, DT)){
            if(!isDeadAfterLoop(candidates[i], successorBB)){
                outs()<<"[Removed -- BB does not dominate all exits]\t"<<*candidates[i]<<"\n";
                candidates.erase(candidates.begin() + i);
                i--;
                continue;
            }
            else{
                outs()<<"[Saved -- instruction dead after loop]\t"<<*candidates[i]<<"\n";
            }
        }

        /*if(!multipleDefinitions(candidates[i])){
            outs()<<"[Removed -- var has multiple definitions]\t"<<*candidates[i]<<"\n";
            candidates.erase(candidates.begin() + i);
            i--;
            continue;
        }*/

        if(!dominatesAllUses(candidates[i], DT)){
            outs()<<"[Removed -- var does not dominate all uses]\t"<<*candidates[i]<<"\n";
            candidates.erase(candidates.begin() + i);
            i--;
            continue;
        }
    }

    outs()<<"\n----- MOVING INSTRUCTIONS TO PREHEADER -----\n";
    BasicBlock* preheader = L.getLoopPreheader();
    if(preheader) {
        outs()<<"[Preheader found]\t"<<*preheader<<'\n';
        for (auto instr : candidates){
            instr->moveBefore(preheader->getTerminator());
            outs()<<"[Moved to preheader]\t"<<*instr<<"\n";
        }
    } else {
        outs()<<"[No preheader found]\n";
    }
    
    

    return PreservedAnalyses::none();
}

