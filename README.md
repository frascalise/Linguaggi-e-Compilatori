<h2>Struttura Cartelle</h2>
- Esercizi: Passi/File/Esempi delle lezioni
<br>
- Assignments: Assignment da consegnare

<hr><hr>

<h3>Installazione del passo File.cpp:</h3>

Aggiungere File.cpp <br>
llvm-root/SRC/llvm/lib/Transforms/Utils/File.cpp

Aggiungere "File.cpp" all'interno del file CMakeLists.txt <br>
llvm-root/SRC/llvm/lib/Transforms/Utils/CMakeLists.txt

Aggiungere File.h:<br>
llvm-root/SRC/llvm/include/llvm/Transforms/Utils/File.h

Modificare PassRegistry.def: <br>
llvm-root/SRC/llvm/lib/Passes/PassRegistry.def

Modificare PassBuilder.cpp aggiungendo il percorso del File.h:<br>
llvm-root/SRC/llvm/lib/Passes/PassBuilder.cpp
