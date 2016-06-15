# !bin/bash

CRETE_BINARY_DIR=$1

cd i386-softmmu \
	&& cp ../op_helper.bc ../crete_helper.bc . \
	&& ln -sf `pwd`/op_helper.bc $CRETE_BINARY_DIR/bin/crete-qemu-1.0-op-helper-i386.bc \
	&& ln -sf `pwd`/crete_helper.bc $CRETE_BINARY_DIR/bin/crete-qemu-1.0-crete-helper-i386.bc \
	&& ln -sf `pwd`/crete-llvm-translator-i386 $CRETE_BINARY_DIR/bin/crete-qemu-1.0-llvm-translator-i386
