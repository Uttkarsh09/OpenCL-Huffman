#!/bin/bash
set -e

clear

rm main
cd ./bin/
rm -rf *

sourceFiles="../src/CLFW/*.cpp ../src/*.cpp"

g++ -w -c ${sourceFiles} -I"/opt/cuda/include/" -I"../include/" -I"../include/CLFW/"
g++ -w -o main *.o -L"/opt/cuda/lib64/" -lOpenCL -lm

cp main ../
cd ..

pwd

./main 
