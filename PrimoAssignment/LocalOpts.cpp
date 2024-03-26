/*
    TODO:
        - [x] Algebraic Identity: x+0 = 0+x = x  | x*1 = 1*x = x
            - [x] Controllare se l'operazione è una Instruction::Mul o Instruction::Add
            - [x] Controllare se esiste una costante e se è uguale ad 1 (Mul) o a 0 (Add)
            - [x] Rimuovere l'istruzione
        - [] Strength Reduction: 15*x = x*15 = (x<<4)-1 | y = x/8 -> y = x>>3
            - [] Controllare se l'operazione è una Instruction::Mul
            - [] Controllare se esiste una costante
            - [] Calcolare se è potenza di due precisa oppure serve una somma/sottrazione
            - [] Creare le istruzioni
        - [] Multi-Instruction Optimization: a=b+1, c=a-1 -> a=b+1, c=b
*/

#include "llvm/Transforms/Utils/LocalOpts.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instructions.h"

using namespace llvm;

bool runOnBasicBlock(BasicBlock &B) {

    for (auto istruzione = B.begin(); istruzione != B.end(); ) {

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
                istruzione++;                                       // Incremento prima l'iteratore del BasicBlock perchè altrimenti farlo dopo incrementerebbe 
                moltiplicazione->replaceAllUsesWith(opAI1);         // qualcosa di eliminato. Successivamente, con replaceAllUsesWith vado a sostituire il valore
                moltiplicazione->eraseFromParent();                 // di X (es. X = Y + 0) con quello di Y. Di conseguenza, tutte le volte che verrà chiamato X
                continue;                                           // andrò a rimpiazzarlo con Y.
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