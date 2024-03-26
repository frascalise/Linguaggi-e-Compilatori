Installazione del passo File.cpp:

Aggiungere File.cpp: 
- llvm-root/SRC/llvm/lib/Transforms/Utils/File.cpp


Aggiungere "File.cpp" all'interno del file CMakeLists.txt
- llvm-root/SRC/llvm/lib/Transforms/Utils/CMakeLists.txt


Aggiungere File.h:
- llvm-root/SRC/llvm/include/llvm/Transforms/Utils/File.h


Modificare PassRegistry.def:
- llvm-root/SRC/llvm/lib/Passes/PassRegistry.def


Modificare PassBuilder.cpp aggiungendo il percorso del File.h:
- llvm-root/SRC/llvm/lib/Passes/PassBuilder.cpp
