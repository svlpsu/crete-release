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
 		-DCRETE_DBG_CURRENT\
 		-DCRETE_DBG_TLB_FILL \
 		-DCRETE_DBG_CALL_STACK \
 		-DCRETE_DBG_INST_BASED_CALL_STACK \
 		-DCRETE_DBG_REPLAY_INTERRUPT \
 		-DCRETE_HELPER \
 		-DCRETE_DBG_TB_GRAPH \
 		-DCRETE_DEP_ANALYSIS" \
 	--extra-cxxflags="-mno-sse3 \
 		-O2 \
        	-DCRETE_CONFIG \
 		-DCRETE_DBG_CALL_STACK \
 		-DCRETE_DBG_INST_BASED_CALL_STACK \
 		-DCRETE_DBG_REPLAY_INTERRUPT \
 		-DCRETE_HELPER \
 		-DCRETE_DBG_TB_GRAPH \
 		-DCRETE_DEP_ANALYSIS"
