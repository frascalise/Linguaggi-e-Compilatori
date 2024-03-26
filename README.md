Installazione del passo File.cpp:

Aggiungere File.cpp: 
llvm-root/SRC/llvm/lib/Transforms/Utils/File.cpp
<br><br>

Aggiungere "File.cpp" all'interno del file CMakeLists.txt
llvm-root/SRC/llvm/lib/Transforms/Utils/CMakeLists.txt
<br><br>

Aggiungere File.h:
llvm-root/SRC/llvm/include/llvm/Transforms/Utils/File.h
<br><br>

Modificare PassRegistry.def:
llvm-root/SRC/llvm/lib/Passes/PassRegistry.def
<br><br>

Modificare PassBuilder.cpp aggiungendo il percorso del File.h:
llvm-root/SRC/llvm/lib/Passes/PassBuilder.cpp
