/*
    TODO:
        - [x] Algebraic Identity: x+0 = 0+x = x  | x*1 = 1*x = x
            - [x] Controllare se l'operazione è una Instruction::Mul o Instruction::Add
            - [x] Controllare se esiste una costante e se è uguale ad 1 (Mul) o a 0 (Add)
            - [x] Rimuovere l'istruzione
        - [] Strength Reduction: 15*x = x*15 = (x<<4)-1 | y = x/8 -> y = x>>3
            - [x] Controllare se l'operazione è una Instruction::Mul o Instruction::SDiv
            - [x] Controllare se esiste una costante
            - [x] Calcolare se è potenza di due precisa oppure serve una somma/sottrazione
                - [] Nel caso calcolare la differenza dello shift
            - [x] Creare le istruzioni
        - [] Multi-Instruction Optimization: a=b+1, c=a-1 -> a=b+1, c=b
*/

#include "llvm/Transforms/Utils/LocalOpts.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instructions.h"

using namespace llvm;

bool runOnBasicBlock(BasicBlock &B) {

    for (auto istruzione = B.begin(); istruzione != B.end(); ) {

        outs() << "ISTRUZIONE: " << *istruzione << "\n";

        bool AIorSR = false;      // flag che impedisce al programma di entrare nell'if di Strength Reduction 
                                  // se si è entrati prima in Algebraic Identity con una moltiplicazione

//      -------------------- Algebraic Identity --------------------
        if (istruzione->getOpcode() == Instruction::Add){
            
            BinaryOperator *addizione = dyn_cast<BinaryOperator>(istruzione);         // Puntatore all'istruzione corrente

            ConstantInt* constAI = nullptr;
            Value* opAI1 = nullptr;
            Value* opAI2 = nullptr;

            for (auto operando = istruzione->op_begin(); operando != istruzione->op_end(); ++operando) {

                if (dyn_cast<ConstantInt>(operando))                // Se nella addizione esiste una costante numerica la salvo,
                    constAI = dyn_cast<ConstantInt>(operando);      // altrimenti vorrà dire che nella addizione ci saranno soltanto    
                else if (!opAI1)                                    // 2 variabili. (e quindi non 1 costante e 1 variabile)           
                    opAI1 = *operando;
                else if (opAI1)
                    opAI2 = *operando;
            }

            if(constAI && constAI->getValue().isZero()){
                istruzione++;                                       // Incremento prima l'iteratore del BasicBlock perchè altrimenti farlo dopo incrementerebbe 
                addizione->replaceAllUsesWith(opAI1);               // qualcosa di eliminato. Successivamente, con replaceAllUsesWith vado a sostituire il valore
                addizione->eraseFromParent();                       // di X (es. X = Y + 0) con quello di Y. Di conseguenza, tutte le volte che verrà chiamato X
                continue;                                           // andrò a rimpiazzarlo con Y.
            }

        } else if (istruzione->getOpcode() == Instruction::Mul){

            BinaryOperator *moltiplicazione = dyn_cast<BinaryOperator>(istruzione);   // Puntatore all'istruzione corrente

            ConstantInt* constAI = nullptr;
            Value* opAI1 = nullptr;
            Value* opAI2 = nullptr;

            for (auto operando = istruzione->op_begin(); operando != istruzione->op_end(); ++operando) {

                if (dyn_cast<ConstantInt>(operando))               // Se nella moltiplicazione esiste una costante numerica la salvo,         
                    constAI = dyn_cast<ConstantInt>(operando);     // altrimenti vorrà dire che nella moltiplicazione ci saranno soltanto        
                else if (!opAI1)                                   // 2 variabili. (e quindi non 1 costante e 1 variabile)                
                    opAI1 = *operando;
                else if (opAI1)
                    opAI2 = *operando;
            }

            if(constAI && constAI->getValue().isOne()){
                AIorSR = true;                                      // impedisco al programma di fare la Strength Reduction

                istruzione++;                                       // Incremento prima l'iteratore del BasicBlock perchè altrimenti farlo dopo incrementerebbe 
                moltiplicazione->replaceAllUsesWith(opAI1);         // qualcosa di eliminato. Successivamente, con replaceAllUsesWith vado a sostituire il valore
                moltiplicazione->eraseFromParent();                 // di X (es. X = Y + 0) con quello di Y. Di conseguenza, tutte le volte che verrà chiamato X
                continue;                                           // andrò a rimpiazzarlo con Y.
            }

        } 
        
//      -------------------- Strength Reduction --------------------
        if ((istruzione->getOpcode() == Instruction::Mul || istruzione->getOpcode() == Instruction::SDiv) && !AIorSR){

            BinaryOperator *operazioneSR = dyn_cast<BinaryOperator>(istruzione);   // Puntatore all'istruzione corrente

            ConstantInt* constSR = nullptr;
            Value* opSR1 = nullptr;
            Value* opSR2 = nullptr;

            for (auto operando = istruzione->op_begin(); operando != istruzione->op_end(); ++operando) {

                if (dyn_cast<ConstantInt>(operando))               // Se nella moltiplicazione esiste una costante numerica la salvo,         
                    constSR = dyn_cast<ConstantInt>(operando);     // altrimenti vorrà dire che nella moltiplicazione ci saranno soltanto        
                else if (!opSR1)                                   // 2 variabili. (e quindi non 1 costante e 1 variabile)                
                    opSR1 = *operando;
                else if (opSR1)
                    opSR2 = *operando;
            }
            
            if (constSR){       // Se l'operazione ha una costante numerica ...

                if (constSR->getValue().isPowerOf2()) {     // ed è una potenza di due...

                    ConstantInt *shift = ConstantInt::get(constSR->getType(), constSR->getValue().exactLogBase2());     // Calcolo lo shift dell'operazione

                    if(istruzione->getOpcode() == Instruction::Mul) {
                        outs() << ">> Strength Reduction Moltiplicazione, creo l'istruzione" << "\n";
                        Instruction *nuovoShift = BinaryOperator::Create(BinaryOperator::Shl, opSR1, shift);            // Creo la nuova operazione

                        nuovoShift->insertAfter(operazioneSR);                          // Inserisco l'istruzione appena creata nella riga successiva all'
                        operazioneSR->replaceAllUsesWith(nuovoShift);                   // operazione che voglio sostituire e rimpiazzo tutti gli usi
                                                                                        // della vecchia operazione con il nuovo shift

                    } else if (istruzione->getOpcode() == Instruction::SDiv) {
                        outs() << ">> Strength Reduction Divisione, creo l'istruzione" << "\n";
                        Instruction *nuovoShift = BinaryOperator::Create(BinaryOperator::LShr, opSR1, shift);            // Creo la nuova operazione

                        nuovoShift->insertAfter(operazioneSR);                          // Inserisco l'istruzione appena creata nella riga successiva all'
                        operazioneSR->replaceAllUsesWith(nuovoShift);                   // operazione che voglio sostituire e rimpiazzo tutti gli usi
                                                                                        // della vecchia operazione con il nuovo shift
                    }
                } else {

                    ConstantInt *shift = ConstantInt::get(constSR->getType(), constSR->getValue().nearestLogBase2());   // Trovo il logaritmo più vicino
                    outs() << "Shift: " << shift->getValue() << "\n";

                    if(istruzione->getOpcode() == Instruction::Mul) {
                        outs() << ">> Strength Reduction Moltiplicazione, creo le istruzioni" << "\n";

                        Instruction *nuovoShift = BinaryOperator::Create(BinaryOperator::Shl, opSR1, shift);            // Creo la nuova operazione

                        nuovoShift->insertAfter(operazioneSR);                          // Inserisco l'istruzione appena creata nella riga successiva all'
                        operazioneSR->replaceAllUsesWith(nuovoShift);                   // operazione che voglio sostituire e rimpiazzo tutti gli usi
                                                                                        // della vecchia operazione con il nuovo shift
                        
                        /*
                            TODO:
                                - Istruzione per calcolare il resto
                        */

                    //    Instruction *nuovoResto = BinaryOperator::Create(BinaryOperator::Add, opSR1, resto); 
                    //    nuovoResto->insertAfter(nuovoShift);  
                        
                    } else if(istruzione->getOpcode() == Instruction::SDiv) {
                        outs() << ">> Strength Reduction Divisione, creo le istruzioni" << "\n";

                        Instruction *nuovoShift = BinaryOperator::Create(BinaryOperator::LShr, opSR1, shift);            // Creo la nuova operazione

                        nuovoShift->insertAfter(operazioneSR);                          // Inserisco l'istruzione appena creata nella riga successiva all'
                        operazioneSR->replaceAllUsesWith(nuovoShift);                   // operazione che voglio sostituire e rimpiazzo tutti gli usi
                                                                                        // della vecchia operazione con il nuovo shift
                        
                        /*
                            TODO:
                                - Istruzione per calcolare il resto
                        */

                    //    Instruction *nuovoResto = BinaryOperator::Create(BinaryOperator::Sub, opSR1, resto); 
                    //    nuovoResto->insertAfter(nuovoShift);                          
                        
                    }
                }
            }

        }



        istruzione++;
    }
    return true;
}

bool runOnFunction(Function &F) {
    bool Transformed = false;

    for (auto Iter = F.begin(); Iter != F.end(); ++Iter) {
        if (runOnBasicBlock(*Iter)) {
        Transformed = true;
        }
    }

    return Transformed;
}

PreservedAnalyses LocalOpts::run(Module &M, ModuleAnalysisManager &AM) {
    for (auto Fiter = M.begin(); Fiter != M.end(); ++Fiter)
        if (runOnFunction(*Fiter))
        return PreservedAnalyses::none();

    return PreservedAnalyses::all();
}