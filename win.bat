@echo off

cls

@REM OpenCL Windows

cd "./bin/"

cl.exe /c /EHsc /std:c++20 /I "C:\KhronosOpenCL\include" /I "../include/" /I "../include/CLFW/" "../src/main.cpp"  "../src/logger.cpp" "../src/HuffmanTree.cpp" "../src/compress.cpp" "../src/CLFW/CLFW.cpp" "../src/CLFW/OclLogger.cpp"
@REM "../src/decompress/*.cpp"

link.exe main.obj CLFW.obj logger.obj OclLogger.obj compress.obj HuffmanTree.obj /LIBPATH:"C:\KhronosOpenCL\lib"

@copy main.exe "../" > nul

cd ../

main.exe
