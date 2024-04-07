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

/*  Verifica che l'istruzione sia di nostro interesse per l'ottimizzazione: ai fini di questo passo è necessario 
    che l'istruzione da ottimizzare contenga una variabile e una costante */
bool optimizable(llvm::BasicBlock::iterator &istruzione, ConstantInt* &constVal, Value* &opVal){
    for (auto operando = istruzione->op_begin(); operando != istruzione->op_end(); ++operando) {
        if (dyn_cast<ConstantInt>(operando))
            constVal = dyn_cast<ConstantInt>(operando); 
        else if (!opVal)          
            opVal = *operando;
    }

    if(constVal && opVal) 
        return true;
    
    return false;
}


bool algebraicIdentity(llvm::BasicBlock::iterator &istruzione){
    ConstantInt* constVal = nullptr;
    Value* opVal = nullptr;

    if(!optimizable(istruzione, constVal, opVal)) return false;

    if (istruzione->getOpcode() == Instruction::Add){
        
        BinaryOperator *addizione = dyn_cast<BinaryOperator>(istruzione);         // Puntatore all'istruzione corrente

        if(constVal->getValue().isZero()){
            outs() << ">> Algebraic Identity [ADD]" << *istruzione << "\n";
            istruzione++;                                       // Incremento prima l'iteratore del BasicBlock perchè altrimenti farlo dopo incrementerebbe 
            addizione->replaceAllUsesWith(opVal);               // qualcosa di eliminato. Successivamente, con replaceAllUsesWith vado a sostituire il valore
            addizione->eraseFromParent();                       // di X (es. X = Y + 0) con quello di Y. Di conseguenza, tutte le volte che verrà chiamato X
            return true;                                           // andrò a rimpiazzarlo con Y.
        }

    } else if (istruzione->getOpcode() == Instruction::Mul){

        BinaryOperator *moltiplicazione = dyn_cast<BinaryOperator>(istruzione);   // Puntatore all'istruzione corrente

        if(constVal->getValue().isOne()){
            outs() << ">> Algebraic Identity [MUL]" << *istruzione << "\n";
            istruzione++;                                       // Incremento prima l'iteratore del BasicBlock perchè altrimenti farlo dopo incrementerebbe 
            moltiplicazione->replaceAllUsesWith(opVal);         // qualcosa di eliminato. Successivamente, con replaceAllUsesWith vado a sostituire il valore
            moltiplicazione->eraseFromParent();                 // di X (es. X = Y + 0) con quello di Y. Di conseguenza, tutte le volte che verrà chiamato X
            return true;                                           // andrò a rimpiazzarlo con Y.
        }
    }


    return false;    
}

bool strenghtReduction(llvm::BasicBlock::iterator &istruzione){
    ConstantInt* constVal = nullptr;
    Value* opVal = nullptr;

    if(!optimizable(istruzione, constVal, opVal)) return false;

    if ((istruzione->getOpcode() == Instruction::Mul || istruzione->getOpcode() == Instruction::SDiv)){

        BinaryOperator *operazioneSR = dyn_cast<BinaryOperator>(istruzione);   // Puntatore all'istruzione corrente

        if (constVal->getValue().isPowerOf2()) {     // ed è una potenza di due...

            ConstantInt *shift = ConstantInt::get(constVal->getType(), constVal->getValue().exactLogBase2());     // Calcolo lo shift dell'operazione

            if(istruzione->getOpcode() == Instruction::Mul) {
                outs() << ">> Strength Reduction [MUL] (perfect power) " << *istruzione << "\n";
                Instruction *nuovoShift = BinaryOperator::Create(BinaryOperator::Shl, opVal, shift);            // Creo la nuova operazione
                istruzione++;
                nuovoShift->insertAfter(operazioneSR);                          // Inserisco l'istruzione appena creata nella riga successiva all'
                operazioneSR->replaceAllUsesWith(nuovoShift);                   // operazione che voglio sostituire e rimpiazzo tutti gli usi
                                                                                // della vecchia operazione con il nuovo shift
                operazioneSR->eraseFromParent();
                return true;

            } else if (istruzione->getOpcode() == Instruction::SDiv) {
                outs() << ">> Strength Reduction [SDIV] (perfect power) " << *istruzione << "\n";
                Instruction *nuovoShift = BinaryOperator::Create(BinaryOperator::LShr, opVal, shift);            // Creo la nuova operazione

                istruzione++;
                nuovoShift->insertAfter(operazioneSR);                          // Inserisco l'istruzione appena creata nella riga successiva all'
                operazioneSR->replaceAllUsesWith(nuovoShift);                   // operazione che voglio sostituire e rimpiazzo tutti gli usi
                                                                                // della vecchia operazione con il nuovo shift
                operazioneSR->eraseFromParent();
                return true;
            }
        } else {

            ConstantInt *shift = ConstantInt::get(constVal->getType(), constVal->getValue().ceilLogBase2());   // Trovo il logaritmo più vicino
            outs() << "Shift: " << shift->getValue() << "\n";

            if(istruzione->getOpcode() == Instruction::Mul) {
                outs() << ">> Strength Reduction [MUL] (not perfect power)" << *istruzione << "\n";

                Instruction *nuovoShift = BinaryOperator::Create(BinaryOperator::Shl, opVal, shift);            // Creo la nuova operazione

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
                uint32_t restoIntero = potenza - constVal->getValue().getLimitedValue();				// valore del resto di tipo int 32
                
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
                return true;                      
                
                
            } else if(istruzione->getOpcode() == Instruction::SDiv) {
                outs() << ">> Strength Reduction Divisione, creo le istruzioni" << "\n";

                Instruction *nuovoShift = BinaryOperator::Create(BinaryOperator::LShr, opVal, shift);            // Creo la nuova operazione

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
                uint32_t restoIntero = potenza - constVal->getValue().getLimitedValue();				// valore del resto di tipo int 32
                
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
                return true;                      
                
            }
        }
    }

    return false;
}

/*  Per ogni istruzione ottimizzabile viene memorizzato nella variabile def il riferimento all'istruzione (unica) che definisce
    la variabile utilizzata. In questo modo, confrontando l'istruzione in fase di ottimizzazione con l'istruzione che definisce la variabile
    utilizzata, è possibile eliminare eventuali passi inutili */
bool multiInstrOpt(llvm::BasicBlock::iterator& istruzione){
    ConstantInt* constVal = nullptr;
    Value* opVal = nullptr;

    if(!optimizable(istruzione, constVal, opVal)) return false;

    /*  Se la variabile non corrisponde ad un'istruzione significa che è presa dai parametri della funzione
        e sicuramente non c'è nulla da ottimizzare */
    Instruction *def = nullptr;
    if(!(def = dyn_cast<Instruction>(opVal))){
        return false;
    }

    BinaryOperator *currentIstr = dyn_cast<BinaryOperator>(istruzione);   // Puntatore all'istruzione corrente

    switch(istruzione->getOpcode()){
        /*
            a=b-1
            c=a+1 
        */
        case Instruction::Add:
            if(def->getOpcode() == Instruction::Sub){
                if (ConstantInt* defConstVal = dyn_cast<ConstantInt>(def->getOperand(1))){
                    if(constVal->getSExtValue() == defConstVal->getSExtValue())
                        outs() << ">> Multi Instruction Optimization [ADD/SUB]" << *istruzione << "\n";
                        istruzione++;
                        currentIstr->replaceAllUsesWith(def->getOperand(0));                  
                        currentIstr->eraseFromParent();
                        return true;      
                }  
            }
            break;

        /*
            a=b+1
            c=a-1
        */
        case Instruction::Sub:
            if(def->getOpcode() == Instruction::Add){
                //qui non so perchè funziona anche se la costante è il primo operatore (giusto, ma non l'ho fatto apposta) (INDAGHERO!)
                if (ConstantInt* defConstVal = dyn_cast<ConstantInt>(def->getOperand(1))){
                    if(constVal->getSExtValue() == defConstVal->getSExtValue())
                        outs() << ">> Multi Instruction Optimization [SUB/ADD]" << *istruzione << "\n";
                        istruzione++;
                        currentIstr->replaceAllUsesWith(def->getOperand(0));                  
                        currentIstr->eraseFromParent();
                        return true;      
                }  
            }
            break;

        /*
            a=b/5
            c=a*5
        */
        case Instruction::Mul:
            if(def->getOpcode() == Instruction::SDiv){
                if (ConstantInt* defConstVal = dyn_cast<ConstantInt>(def->getOperand(1))){
                    if(constVal->getSExtValue() == defConstVal->getSExtValue())
                        outs() << ">> Multi Instruction Optimization [SDIV/MUL]" << *istruzione << "\n";
                        istruzione++;
                        currentIstr->replaceAllUsesWith(def->getOperand(0));                  
                        currentIstr->eraseFromParent();
                        return true;      
                }  
            }
            break;

        /*
            a=5*b
            c=a/5
        */
        case Instruction::SDiv:
            if(def->getOpcode() == Instruction::Mul){
                if (ConstantInt* defConstVal = dyn_cast<ConstantInt>(def->getOperand(1))){
                    if(constVal->getSExtValue() == defConstVal->getSExtValue())
                        outs() << ">> Multi Instruction Optimization [MUL/SDIV]" << *istruzione << "\n";
                        istruzione++;
                        currentIstr->replaceAllUsesWith(def->getOperand(0));                  
                        currentIstr->eraseFromParent();
                        return true;      
                }  
            }
            break;
        default:
            break;
            
    }

    return false;
}

bool runOnBasicBlock(BasicBlock &B) {

    llvm::BasicBlock::iterator istruzione = B.begin();
    while (istruzione != B.end()) {

        outs() << "ISTRUZIONE: " << *istruzione << "\n";

//      -------------------- Algebraic Identity --------------------
        if(algebraicIdentity(istruzione)){
            continue;
        }
//      -------------------- Strength Reduction --------------------
        /*FIXME: questo mi cattura tutte le mul e sdiv e non si arriva mai nell'ultima ottimizzazione, possiamo risolvere
            facendo più cicli oppure cambiando questa funzione in modo che prenda solo le istruzioni con la costante vicina alla potenza perfetta, ciao*/
        /*if(strenghtReduction(istruzione)){
            continue;
        }*/
//      ----------------- Multi Instruction Opt --------------------
        if(multiInstrOpt(istruzione)){
            continue;
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

