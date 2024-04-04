#include "llvm/Transforms/Utils/LocalOpts.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instructions.h"
 
using namespace llvm;

bool runOnBasicBlock(BasicBlock &B) {
    
    for (auto istruzione = B.begin(); istruzione != B.end(); ++istruzione) {   // Utilizzato per scorrere l'intero BasicBlock tra le varie istruzioni

        if (istruzione->getOpcode() == Instruction::Mul) {      // Controllo se l'istruzione è una moltiplicazione
            BinaryOperator *moltiplicazione = dyn_cast<BinaryOperator>(istruzione);
            
            istruzione->print(outs());  // Stampa l'istruzione
            
            int i = 0;
            ConstantInt *costanteNumerica = nullptr;    // Gli operandi della moltiplicazione vengono settati a nullptr perchè cosi vengono
            Value *operandoMoltiplicazione1 = nullptr;  // resettati ad ogni iterazione.
            Value *operandoMoltiplicazione2 = nullptr;  // Non è detto che questo Value venga utilizzato

            for (auto operando = istruzione->op_begin(); operando != istruzione->op_end(); ++operando) {

                if (dyn_cast<ConstantInt>(operando))                            // Se nella moltiplicazione esiste una costante numerica la salvo,
                    costanteNumerica = dyn_cast<ConstantInt>(operando);         // altrimenti vorrà dire che nella moltiplicazione ci sono soltanto 
                else if (i == 0)                                                // 2 variabili. (e quindi non 1 costante e 1 variabile)
                    operandoMoltiplicazione1 = *operando;
                else if (i == 1)
                    operandoMoltiplicazione2 = *operando;

                i++;
            }
            if (costanteNumerica && costanteNumerica->getValue().isPowerOf2()){      // Se constanteNumerica ha valore ed è una potenza di 2
                outs() << "\n\t COSTANTE: " << costanteNumerica->getValue() << "\n \t OPERANDO: " << *operandoMoltiplicazione1 << "\n";

                // ConstantInt::get() ritorna un valore di tipo ConstantInt
                ConstantInt *shiftSinistra = ConstantInt::get(costanteNumerica->getType(), costanteNumerica->getValue().exactLogBase2());

                outs() << costanteNumerica->getValue() << " è una potenza di due, ovvero 2^ " << shiftSinistra->getValue() << "\n";

                // Creo la nuova istruzione di shift
                Instruction *nuovoShift = BinaryOperator::Create(BinaryOperator::Shl, operandoMoltiplicazione1, shiftSinistra);

                nuovoShift->insertAfter(moltiplicazione);           // Inserisco l'istruzione appena creata nella riga successiva alla
                moltiplicazione->replaceAllUsesWith(nuovoShift);    // moltiplicazione che voglio sostituire
            }
        }
    }

    // Preleviamo le prime due istruzioni del BB
    Instruction &Inst1st = *B.begin(), &Inst2nd = *(++B.begin());

    // L'indirizzo della prima istruzione deve essere uguale a quello del
    // primo operando della seconda istruzione (per costruzione dell'esempio)
    assert(&Inst1st == Inst2nd.getOperand(0));

    // Stampa la prima istruzione
    // outs() << "PRIMA ISTRUZIONE: " << Inst1st << "\n";
    // Stampa la prima istruzione come operando
    // outs() << "COME OPERANDO: ";
    // Inst1st.printAsOperand(outs(), false);
    // outs() << "\n";

    // User-->Use-->Value
    // outs() << "I MIEI OPERANDI SONO:\n";
    for (auto *Iter = Inst1st.op_begin(); Iter != Inst1st.op_end(); ++Iter) {
        Value *Operand = *Iter;

        if (Argument *Arg = dyn_cast<Argument>(Operand)) {
        // outs() << "\t" << *Arg << ": SONO L'ARGOMENTO N. " << Arg->getArgNo() << " DELLA FUNZIONE " << Arg->getParent()->getName() << "\n";
        }
        if (ConstantInt *C = dyn_cast<ConstantInt>(Operand)) {
        // outs() << "\t" << *C << ": SONO UNA COSTANTE INTERA DI VALORE " << C->getValue() << "\n";
        }
    }

    // outs() << "LA LISTA DEI MIEI USERS:\n";
    for (auto Iter = Inst1st.user_begin(); Iter != Inst1st.user_end(); ++Iter) {
        // outs() << "\t" << *(dyn_cast<Instruction>(*Iter)) << "\n";
    }

    // outs() << "E DEI MIEI USI (CHE E' LA STESSA):\n";
    for (auto Iter = Inst1st.use_begin(); Iter != Inst1st.use_end(); ++Iter) {
        // outs() << "\t" << *(dyn_cast<Instruction>(Iter->getUser())) << "\n";
    }

    // Manipolazione delle istruzioni
    Instruction *NewInst = BinaryOperator::Create(Instruction::Add, Inst1st.getOperand(0), Inst1st.getOperand(0));

    NewInst->insertAfter(&Inst1st);
    // Si possono aggiornare le singole references separatamente?
    // Controlla la documentazione e prova a rispondere.
    Inst1st.replaceAllUsesWith(NewInst);

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
