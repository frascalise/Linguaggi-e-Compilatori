Installazione del passo File.cpp:

1. Aggiungere File.cpp: 
- llvm-root/SRC/llvm/lib/Transforms/Utils/File.cpp

2. Aggiungere "File.cpp" all'interno del file CMakeLists.txt
- llvm-root/SRC/llvm/lib/Transforms/Utils/CMakeLists.txt

3. Aggiungere File.h:
- llvm-root/SRC/llvm/include/llvm/Transforms/Utils/File.h

4. Modificare PassRegistry.def:
- llvm-root/SRC/llvm/lib/Passes/PassRegistry.def

5. Modificare PassBuilder.cpp aggiungendo il percorso del File.h:
- llvm-root/SRC/llvm/lib/Passes/PassBuilder.cpp