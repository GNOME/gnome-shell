#!/bin/bash

SRCDIR=$(realpath $(dirname $0)/..)
OUTDIR=$(mktemp --directory --tmpdir=$SRCDIR)
trap "rm -rf $OUTDIR" EXIT

# Turn ```javascript``` code snippets in the
# style guide into .js files in $OUTDIR
awk --assign dir=$OUTDIR -- '
  BEGIN {
    do_print = 0;
    cur = 0;
  }

  # end of code snippet
  /```$/ {
    do_print = 0;
    cur++;
  }

  do_print {
    # remove one level of indent
    sub("    ","")

    # the following are class snippets, turn them
    # into functions to not confuse eslint
    sub("moveActor","function moveActor")
    sub("desaturateActor","function desaturateActor")

    # finally, append to the currently generated .js file
    print >> dir "/" cur ".js";
  }

  # start of code snippet
  /```javascript$/ {
    do_print = 1;
  }
' docs/js-coding-style.md

eslint \
  --rule 'no-undef: off' \
  --rule 'no-unused-vars: off' \
  --rule 'no-invalid-this: off' $OUTDIR/*.js
