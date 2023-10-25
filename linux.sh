#!/bin/bash
set -e

clear

cd ./bin/
rm -rf *

sourceFiles="../src/CLFW/*.cpp ../src/*.cpp"

g++ -Wall -c ${sourceFiles} -I"/opt/cuda/include/" -I"../include/" -I"../include/CLFW/"
g++ -Wall -o main *.o -L"/opt/cuda/lib64/" -lOpenCL -lm

cp main ../
cd ..

pwd

./main
