#!/bin/sh

UNIT_TEST=`basename $0`

echo "Running ./test-interactive $UNIT_TEST $@"
echo ""
echo "NOTE: For debugging purposes, you can run this single test as follows:"
echo "$ libtool --mode=execute \\"
echo "          gdb --eval-command=\"b `echo $UNIT_TEST|tr '-' '_'`_main\" \\"
echo "              --args ./test-interactive $UNIT_TEST" 

./test-interactive $UNIT_TEST "$@"

