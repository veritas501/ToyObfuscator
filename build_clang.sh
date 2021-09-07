#!/bin/bash

# check argc
if [ $# -lt 1 ]
then
    echo "usage $0 <llvm-project dir>"
    exit 1
fi
src_dir=$1
self_dir=$(cd "$(dirname "$0")";pwd)

# do some check
if [ ! -d "$src_dir" ]; then
    echo "$src_dir is not dir"
    exit 1
fi
if [ ! -d "$src_dir/llvm/lib/Transforms" ]; then
    echo "$src_dir is not llvm-project"
    exit 1
fi

# copy file
mkdir -p "$src_dir/llvm/lib/Transforms/ToyObfuscator"
cp -r "$self_dir/src/"* "$src_dir/llvm/lib/Transforms/ToyObfuscator/"
cp -r "$self_dir/include/"* "$src_dir/llvm/include/"

# patch CMakeLists.txt
sed -i "s/add_library/add_llvm_library/g" "$src_dir/llvm/lib/Transforms/ToyObfuscator/CMakeLists.txt"
sed -i "s/MODULE//g" "$src_dir/llvm/lib/Transforms/ToyObfuscator/CMakeLists.txt"

# apply some patch
pushd "$src_dir"
# git clone https://github.com/llvm/llvm-project.git --depth 1 -b llvmorg-10.0.1
git apply "$self_dir/clang_patch.diff"
popd