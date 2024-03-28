<h2>Installazione del passo File.cpp:</h2>

Aggiungere File.cpp: <br>
- llvm-root/SRC/llvm/lib/Transforms/Utils/File.cpp
<hr>
Aggiungere "File.cpp" all'interno del file CMakeLists.txt: <br>
- llvm-root/SRC/llvm/lib/Transforms/Utils/CMakeLists.txt
<hr>
Aggiungere File.h: <br>
- llvm-root/SRC/llvm/include/llvm/Transforms/Utils/File.h
<hr>
Modificare PassRegistry.def: <br>
- llvm-root/SRC/llvm/lib/Passes/PassRegistry.def
<hr>
Modificare PassBuilder.cpp aggiungendo il percorso del File.h: <br>
- llvm-root/SRC/llvm/lib/Passes/PassBuilder.cpp
