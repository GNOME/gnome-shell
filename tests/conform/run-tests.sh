#!/bin/sh

BINARY=$1
shift

TMP=`./$BINARY -l -m thorough | grep '^/' | sed -e s/_/-/g`
for i in $TMP
do
  TESTS="$TESTS wrappers/`basename $i`"
done

exec gtester "$@" $TESTS
