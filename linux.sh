#!/bin/bash
set -e

clear

cd ./bin/
rm -rf *

g++ -w -c ../src/CLFW/*.cpp ../src/*.cpp ../src/compress/*.cpp ../src/decompress/*.cpp  -I"/opt/cuda/include/" -I"../include/" -I"../include/CLFW/"
g++ -o main *.o -L"/opt/cuda/lib64/" -lOpenCL -lm

cp main ../
cd ..

pwd

./main
