#!/bin/sh

UNIT_TEST=`basename $0`
UNIT_TEST_PATH=`./test-conformance -l -m thorough |grep "$UNIT_TEST\$"`

echo "Running: gtester -p $UNIT_TEST_PATH ./test-conformance"
echo ""
gtester -p $UNIT_TEST_PATH ./test-conformance

echo ""
echo "NOTE: For debugging purposes, you can run this single test as follows:"
echo "$ libtool --mode=execute \\"
echo "          gdb --eval-command=\"b $UNIT_TEST\" \\"
echo "          --args ./test-conformance -p $UNIT_TEST_PATH"

