Installazione del passo File.cpp:

Aggiungere File.cpp: 
- llvm-root/SRC/llvm/lib/Transforms/Utils/File.cpp
_______________________

Aggiungere "File.cpp" all'interno del file CMakeLists.txt
- llvm-root/SRC/llvm/lib/Transforms/Utils/CMakeLists.txt
_______________________

Aggiungere File.h:
- llvm-root/SRC/llvm/include/llvm/Transforms/Utils/File.h
_______________________

Modificare PassRegistry.def:
- llvm-root/SRC/llvm/lib/Passes/PassRegistry.def
_______________________

Modificare PassBuilder.cpp aggiungendo il percorso del File.h:
- llvm-root/SRC/llvm/lib/Passes/PassBuilder.cpp
