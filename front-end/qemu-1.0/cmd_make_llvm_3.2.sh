# !bin/bash
# Build llvm first, which is required as dependency for building
# QEMU with llvm backend.
# clang/clang++ 3.2 is required, and should be put in the PATH.
cd ../deps/llvm/
tar zxvf llvm-3.2.src.tar.gz
cd llvm-3.2.src/
echo "Build llvm first"
echo "[llvm] Configure"
./configure --enable-jit --enable-optimized
echo "[llvm] Configure is done"
echo "[llvm] Make"
make clean
make -j8 ENABLE_OPTIMIZED=1 REQUIRES_RTTI=1
echo "[llvm] Make is done"
