#!/bin/bash
# type './build.sh'  for release build
# type './build.sh debug'  for debug build

#########
# Please change the following home directories of your LLVM builds
########
LLVMRELEASE=/home/fuzz/code/llvm-11.0.1/build
LLVMDEBUG=/home/fuzz/code/llvm-11.0.1/build

if [[ $1 == 'debug' ]]
then
BuildTY='Debug'
export LLVM_DIR=$LLVMDEBUG
else
BuildTY='Release'
export LLVM_DIR=$LLVMRELEASE
fi

export PATH=$LLVM_DIR/bin:$PATH
Build=$BuildTY'-build'

rm -rf $Build
mkdir $Build
cd $Build

if [[ $1 == 'debug' ]]
then
cmake -D CMAKE_BUILD_TYPE:STRING=Debug ../
else
cmake ../
fi
cmake ../
make -j24

