<h2>Installazione del passo File.cpp:</h2>

Aggiungere File.cpp: 
- llvm-root/SRC/llvm/lib/Transforms/Utils/File.cpp
<hr>
Aggiungere "File.cpp" all'interno del file CMakeLists.txt
- llvm-root/SRC/llvm/lib/Transforms/Utils/CMakeLists.txt
<hr>
Aggiungere File.h:
- llvm-root/SRC/llvm/include/llvm/Transforms/Utils/File.h
<hr>
Modificare PassRegistry.def:
- llvm-root/SRC/llvm/lib/Passes/PassRegistry.def
<hr>
Modificare PassBuilder.cpp aggiungendo il percorso del File.h:
- llvm-root/SRC/llvm/lib/Passes/PassBuilder.cpp
