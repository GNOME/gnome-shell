#!/bin/sh

TEST_BINARY=$1
shift

SYMBOL_PREFIX=$1
shift

UNIT_TEST=$1
shift

test -z ${UNIT_TEST} && {
        echo "Usage: $0 UNIT_TEST"
        exit 1
}

BINARY_NAME=`basename $TEST_BINARY`
UNIT_TEST=`echo $UNIT_TEST|sed 's/-/_/g'`

echo "Running: ./$BINARY_NAME ${UNIT_TEST} $@"
echo ""
COGL_TEST_VERBOSE=1 $TEST_BINARY ${UNIT_TEST} "$@"
exit_val=$?

if test $exit_val -eq 0; then
  echo "OK"
fi

echo ""
echo "NOTE: For debugging purposes, you can run this single test as follows:"
echo "$ libtool --mode=execute \\"
echo "          gdb --eval-command=\"start\" --eval-command=\"b ${UNIT_TEST#${SYMBOL_PREFIX}}\" \\"
echo "          --args ./$BINARY_NAME ${UNIT_TEST}"
echo "or:"
echo "$ env G_SLICE=always-malloc \\"
echo "  libtool --mode=execute \\"
echo "          valgrind ./$BINARY_NAME ${UNIT_TEST}"

exit $exit_val
