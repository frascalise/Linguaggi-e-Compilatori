/*
    TODO:
        - [x] Algebraic Identity: x+0 = 0+x = x  | x*1 = 1*x = x
            - [x] Controllare se l'operazione è una Instruction::Mul o Instruction::Add
            - [x] Controllare se esiste una costante e se è uguale ad 1 (Mul) o a 0 (Add)
            - [x] Rimuovere l'istruzione
        - [] Strength Reduction: 15*x = x*15 = (x<<4)-x | y = x/8 -> y = x>>3
            - [x] Controllare se l'operazione è una Instruction::Mul o Instruction::SDiv
            - [x] Controllare se esiste una costante
            - [x] Calcolare se è potenza di due precisa oppure serve una somma/sottrazione
                - [] Nel caso calcolare la differenza dello shift
            - [x] Creare le istruzioni
        - [] Multi-Instruction Optimization: a=b+1, c=a-1 -> a=b+1, c=b
*/

#include "llvm/Transforms/Utils/LocalOpts.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/InstrTypes.h"
// L'include seguente va in LocalOpts.h
#include <llvm/IR/Constants.h>


using namespace llvm;

bool runOnBasicBlock(BasicBlock &B) {

    for (auto istruzione = B.begin(); istruzione != B.end(); ) {

        outs() << "ISTRUZIONE: " << *istruzione << "\n";

        bool AIorSR = false;      // flag che impedisce al programma di entrare nell'if di Strength Reduction 
                                  // se si è entrati prima in Algebraic Identity con una moltiplicazione

//      -------------------- Algebraic Identity --------------------
        if (istruzione->getOpcode() == Instruction::Add) {
            
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

                    ConstantInt *shift = ConstantInt::get(constSR->getType(), constSR->getValue().ceilLogBase2());   // Trovo il logaritmo più vicino
                    outs() << "Shift: " << shift->getValue() << "\n";

                    if(istruzione->getOpcode() == Instruction::Mul) {
                        outs() << ">> Strength Reduction Moltiplicazione, creo le istruzioni" << "\n";

                        Instruction *nuovoShift = BinaryOperator::Create(BinaryOperator::Shl, opSR1, shift);            // Creo la nuova operazione

                        nuovoShift->insertAfter(operazioneSR);                          // Inserisco l'istruzione appena creata nella riga successiva all'
                                                                                        // della vecchia operazione con il nuovo shift
                        LLVMContext &context = shift->getContext();						// contesto necessario per funzioni successive

                        // calcolo del resto
                        APInt shiftValue = shift->getValue();
						uint32_t potenza = 1;
						for (auto i = 0; i < shiftValue.getLimitedValue(); i++) {		// getLimitedValue ritorna lo stesso valore di shiftValue (APInt) con tipo int 
							potenza *= 2;												// calcolo il valore dello shift aggiunto precedentemente
						}
                        
                        // FIXME: Sistemare questa riga
                        uint32_t restoIntero = potenza - constSR->getValue().getLimitedValue();				// valore del resto di tipo int 32
                        
                        // NOTE: Non si riesce a calcolare il valore da sottrarre/aggiungere poichè il valore numerico
                        // non si può prendere in quanto appartiene ad una istruzione che non è stata ancora eseguita
                        // e di conseguenza non esiste. L'unico modo per farlo è se la costante numerica è vicino a quella
                        // della potenza calcolata (vicino di 1, quindi x*15 -> x*16 - x)

                        Type *int32Type = Type::getInt32Ty(context);
						Constant *restoConstant = ConstantInt::get(int32Type, APInt(32, restoIntero));		// valore del resto in una variabile di tipo Constant, necessario per creazione
																											// dell'istruzione

						Instruction *sottrazioneResto = BinaryOperator::Create(BinaryOperator::Sub, nuovoShift, restoConstant);		// istruzione di sottrazione del resto
						

						istruzione++;
						sottrazioneResto->insertAfter(nuovoShift);
                        operazioneSR->replaceAllUsesWith(sottrazioneResto);                   // operazione che voglio sostituire e rimpiazzo tutti gli usi
						operazioneSR->eraseFromParent();
						continue;                      
                        
                        
                    } else if(istruzione->getOpcode() == Instruction::SDiv) {
                        outs() << ">> Strength Reduction Divisione, creo le istruzioni" << "\n";

                        Instruction *nuovoShift = BinaryOperator::Create(BinaryOperator::LShr, opSR1, shift);            // Creo la nuova operazione

                        nuovoShift->insertAfter(operazioneSR);                          // Inserisco l'istruzione appena creata nella riga successiva all'
                                                                                        // della vecchia operazione con il nuovo shift
                        
                        LLVMContext &context = shift->getContext();						//contesto necessario per funzioni successive

                        // calcolo del resto
                        APInt shiftValue = shift->getValue();
						uint32_t potenza = 1;
						for (auto i = 0; i < shiftValue.getLimitedValue(); i++) {		// getLimitedValue ritorna lo stesso valore di shiftValue (APInt) con tipo int 
							potenza *= 2;												// calcolo il valore dello shift aggiunto precedentemente
						}
                        
                        // FIXME: Sistemare questa riga
                        uint32_t restoIntero = potenza - constSR->getValue().getLimitedValue();				// valore del resto di tipo int 32
                        
                        // NOTE: Non si riesce a calcolare il valore da sottrarre/aggiungere poichè il valore numerico
                        // non si può prendere in quanto appartiene ad una istruzione che non è stata ancora eseguita
                        // e di conseguenza non esiste. L'unico modo per farlo è se la costante numerica è vicino a quella
                        // della potenza calcolata (vicino di 1, quindi x*15 -> x*16 - x)

                        Type *int32Type = Type::getInt32Ty(context);
						Constant *restoConstant = ConstantInt::get(int32Type, APInt(32, restoIntero));		// valore del resto in una variabile di tipo Constant, necessario per creazione
																											// dell'istruzione

						Instruction *addizioneResto = BinaryOperator::Create(BinaryOperator::Add, nuovoShift, restoConstant);		// istruzione di addizione del resto
						
						istruzione++;
						addizioneResto->insertAfter(nuovoShift);
                        operazioneSR->replaceAllUsesWith(addizioneResto);                   // operazione che voglio sostituire e rimpiazzo tutti gli usi
						operazioneSR->eraseFromParent();
						continue;                      
                        
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


PreservedAnalyses LocalOpts::run(Module &M,ModuleAnalysisManager &AM) {
  for (auto Fiter = M.begin(); Fiter != M.end(); ++Fiter)
    if (runOnFunction(*Fiter))
      return PreservedAnalyses::none();
  
  return PreservedAnalyses::all();
}

