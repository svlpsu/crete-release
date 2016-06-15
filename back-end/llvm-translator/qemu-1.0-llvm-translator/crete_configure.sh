#!/bin/bash

CRETE_SOURCE_DIR=$1
CRETE_BINARY_DIR=$2

./configure \
 	--enable-tcg-interpreter \
 	--enable-llvm \
	--with-llvm=$CRETE_BINARY_DIR/lib/llvm/llvm-3.2-prefix/src/llvm-3.2/Release+Asserts/ \
 	--target-list=i386-softmmu,x86_64-softmmu \
	--extra-ldflags="-L$CRETE_BINARY_DIR/bin" \
 	--extra-cflags="-mno-sse3 \
 		-O2 \
 		-I$CRETE_SOURCE_DIR/lib/include \
        	-DCRETE_CONFIG \
 		-DDBG_BO_CURRENT \
 		-DDBG_BO_TLB_FILL \
 		-DBCT_RT_DUMP \
 		-DCONFIG_HAVLICEK \
 		-DDBG_BO_MEM_MONITOR \
 		-DDBG_BO_CALL_STACK \
 		-DDBG_BO_REPLAY_INTERRUPT \
 		-DCRETE_HELPER \
 		-DTCG_LLVM_OFFLINE \
 		-DHAVLICEK_TB_GRAPH \
 		-DHAVLICEK_DBG_FP" \
 	--extra-cxxflags="-mno-sse3 \
 		-O2 \
        	-DCRETE_CONFIG \
 		-DDBG_BO_CURRENT \
 		-DDBG_BO_MEM_MONITOR \
 		-DDBG_BO_CALL_STACK \
 		-DDBG_BO_REPLAY_INTERRUPT \
 		-DCRETE_HELPER \
 		-DHAVLICEK_TB_GRAPH \
 		-DTCG_LLVM_OFFLINE \
 		-DHAVLICEK_DBG_FP"

