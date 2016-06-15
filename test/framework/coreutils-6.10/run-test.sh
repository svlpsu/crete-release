#!/bin/sh

CRETE_BINARY_DIR=$1

export PATH=$PATH:$CRETE_BINARY_DIR/bin
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$CRETE_BINARY_DIR/bin

TIMEOUT=45000

crete-dispatch -c crete.dispatch.xml &
DISPATCH_PID=$!
crete-vm-node -c crete.vm-node.xml &
VM_NODE_PID=$!
crete-svm-node -c crete.svm-node.xml &
SVM_NODE_PID=$!

sleep $TIMEOUT
kill -9 $DISPATCH_PID
kill -9 $VM_NODE_PID
kill -9 $SVM_NODE_PID

for dir in dispatch/last
do
	prog=${dir%.*}
	prog=${prog##*.}

	crete-coverage -t "dispatch/last/$dir/test-case" -c config.xml -e "coreutils-6.10/src/$prog" > "$prog.coverage"

	$prog >> coverages.txt
	cat "$prog.coverage" >> coverages.txt
done

diff coverages.txt expected_coverages.txt

