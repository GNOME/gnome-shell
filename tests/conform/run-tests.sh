#!/bin/sh

BINARY=$1
shift

TMP=`./$BINARY -l -m thorough | grep '^/' | sed -e s/_/-/g`
for i in $TMP
do
  TESTS="$TESTS wrappers/`basename $i`"
done

G_DEBUG=gc-friendly MALLOC_CHECK_=2 MALLOC_PERTURB_=$((${RANDOM:-256} % 256)) gtester "$@" $TESTS
