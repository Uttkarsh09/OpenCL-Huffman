@echo off

cls


cd "./cpu-bin/"

cl.exe /c /EHsc /std:c++20 /I "../include/" "../src/logger.cpp" "../src/HuffmanTree.cpp" "../CPU/mainCPU.cpp" "../CPU/decompressCPU.cpp" "../CPU/compressCPU.cpp"

link.exe mainCPU.obj logger.obj decompressCPU.obj HuffmanTree.obj compressCPU.obj

@REM @copy main.exe "../" > nul


mainCPU.exe
cd ../
