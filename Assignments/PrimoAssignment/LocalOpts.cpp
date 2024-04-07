/*
    TODO:
        - [x] Algebraic Identity: x+0 = 0+x = x  | x*1 = 1*x = x
            - [x] Controllare se l'operazione è una Instruction::Mul o Instruction::Add
            - [x] Controllare se esiste una costante e se è uguale ad 1 (Mul) o a 0 (Add)
            - [x] Rimuovere l'istruzione
        - [x] Strength Reduction: 15*x = x*15 = (x<<4)-x | y = x/8 -> y = x>>3
            - [x] Controllare se l'operazione è una Instruction::Mul o Instruction::SDiv
            - [x] Controllare se esiste una costante
            - [x] Calcolare se è potenza di due precisa oppure serve una somma/sottrazione
                - [x] Nel caso calcolare la differenza dello shift
            - [x] Creare le istruzioni
        - [x] Multi-Instruction Optimization: a=b+1, c=a-1 -> a=b+1, c=b
*/

#include "llvm/Transforms/Utils/LocalOpts.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/InstrTypes.h"
// L'include seguente va in LocalOpts.h
#include <llvm/IR/Constants.h>


using namespace llvm;

/*  Verifica che l'istruzione sia di nostro interesse per l'ottimizzazione: ai fini di questo passo è necessario 
    che l'istruzione da ottimizzare contenga una variabile e una costante */
bool optimizable(Instruction* istruzione, ConstantInt* &constVal, Value* &opVal){
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

    if(!optimizable(&*istruzione, constVal, opVal)) return false;

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

    if(!optimizable(&*istruzione, constVal, opVal)) return false;

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

            ConstantInt *shift = ConstantInt::get(constVal->getType(), constVal->getValue().nearestLogBase2());   // Trovo il logaritmo più vicino
			bool add=false;


            //calcolo del resto
            APInt shiftValue = shift->getValue();
			uint32_t potenza = 1;
			for (auto i = 0; i < shiftValue.getSExtValue(); i++) {		
				potenza *= 2;												
			}
            
			uint32_t valInteroCostante=constVal->getValue().getSExtValue();
			uint32_t restoIntero;
            if (potenza>valInteroCostante)
            	restoIntero=potenza-valInteroCostante;				//valore del resto di tipo int 32
            else{
            	restoIntero=valInteroCostante-potenza;
            	add=true;
            }

			if(restoIntero==1 && istruzione->getOpcode() == Instruction::Mul) {

		        Instruction *nuovoShift = BinaryOperator::Create(BinaryOperator::Shl, opVal, shift);            // Creo la nuova operazione

		        nuovoShift->insertAfter(operazioneSR);                          // Inserisco l'istruzione appena creata nella riga successiva all'
		                                                                        // della vecchia operazione con il nuovo shift
		        
		        
				Instruction *istruzioneResto;
				if(add)
					istruzioneResto = BinaryOperator::Create(BinaryOperator::Add, nuovoShift, opVal);		//istruzione di addizione del resto
				else
					istruzioneResto = BinaryOperator::Create(BinaryOperator::Sub, nuovoShift, opVal);		//istruzione di sottrazione del resto
				
                outs() << ">> Strength Reduction [MUL]" << *istruzione << "\n";
				istruzione++;
				istruzioneResto->insertAfter(nuovoShift);
		        operazioneSR->replaceAllUsesWith(istruzioneResto);                   // operazione che voglio sostituire e rimpiazzo tutti gli usi
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
bool multiInstrOpt(llvm::BasicBlock::iterator &istruzione){
    ConstantInt* constVal = nullptr;
    Value* opVal = nullptr;

    if(!optimizable(&*istruzione, constVal, opVal)) return false;

    /*  Se la variabile non corrisponde ad un'istruzione significa che è presa dai parametri della funzione
        e sicuramente non c'è nulla da ottimizzare */
    Instruction *def = nullptr;
    if(!(def = dyn_cast<Instruction>(opVal))){
        return false;
    }

    ConstantInt* defConstVal = nullptr;
    Value* defOpVal = nullptr;

    BinaryOperator *currentIstr = dyn_cast<BinaryOperator>(istruzione);   // Puntatore all'istruzione corrente

    /*  In caso l'istruzione sia una sub o una sdiv la condizione per l'ottimizzazione è diversa da add e mul in quanto sub e sdiv possono 
        "reversare" una add e una mul indipendentemente dall'ordine in cui compaiono costante e variabile (commutative). Per add e mul, invece,
        non vale questa proprietà perchè possono reversare solo sub e sdiv in cui la costante compare come secondo operatore */
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
                if (optimizable(def, defConstVal, defOpVal)){
                    if(constVal->getSExtValue() == defConstVal->getSExtValue())
                        outs() << ">> Multi Instruction Optimization [SUB/ADD]" << *istruzione << "\n";
                        istruzione++;
                        currentIstr->replaceAllUsesWith(defOpVal);                  
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
                if (optimizable(def, defConstVal, defOpVal)){
                    if(constVal->getSExtValue() == defConstVal->getSExtValue())
                        outs() << ">> Multi Instruction Optimization [MUL/SDIV]" << *istruzione << "\n";
                        istruzione++;
                        currentIstr->replaceAllUsesWith(defOpVal);                  
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
        if(algebraicIdentity(istruzione)){
            continue;
        }

        if(strenghtReduction(istruzione)){
            continue;
        }

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

