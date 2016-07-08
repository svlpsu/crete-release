#!/bin/sh

CRETE_SOURCE_DIR=$2

export PATH=$PATH:$CRETE_BINARY_DIR/bin
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$CRETE_BINARY_DIR/bin

DISPATCH=crete-dispatch
VM_NODE=crete-vm-node
SVM_NODE=crete-svm-node

TIMEOUT=300

$DISPATCH -c crete.dispatch.xml &
DISPATCH_PID=$!
$VM_NODE -c crete.vm-node.xml &
VM_NODE_PID=$!
$SVM_NODE -c crete.svm-node.xml &
SVM_NODE_PID=$!

sleep $TIMEOUT
kill -9 $DISPATCH_PID
kill -9 $VM_NODE_PID
kill -9 $SVM_NODE_PID

rm -f coverages.txt

for dir in dispatch/last
do
	prog=${dir%.*}
	prog=${prog##*.}

	$prog >> coverages.txt
	crete-coverage -t "dispatch/last/$dir/test-case" -c config.xml -e "coreutils-6.10/src/$prog" > /dev/null
	gcov -b "coreutils-6.10/src/$prog" >> coverages.txt
done

diff coverages.txt expected_coverages.txt > coverages.diff
cat coverages.diff

