<h2>Installazione del passo File.cpp:</h2>

<b>Aggiungere File.cpp</b> <br>
llvm-root/SRC/llvm/lib/Transforms/Utils/File.cpp
  
<hr>

<b>Aggiungere "File.cpp" all'interno del file CMakeLists.txt</b> <br>
llvm-root/SRC/llvm/lib/Transforms/Utils/CMakeLists.txt
  
<hr>

<b>Aggiungere File.h:</b> <br>
llvm-root/SRC/llvm/include/llvm/Transforms/Utils/File.h

<hr>

<b>Modificare PassRegistry.def:</b> <br>
llvm-root/SRC/llvm/lib/Passes/PassRegistry.def

<hr>

<b>Modificare PassBuilder.cpp aggiungendo il percorso del File.h:</b> <br>
llvm-root/SRC/llvm/lib/Passes/PassBuilder.cpp
