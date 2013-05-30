#!/bin/bash

ENVIRONMENT_CONFIG=$1
shift

TEST_BINARY=$1
shift

. $ENVIRONMENT_CONFIG

set +m

trap "" ERR
trap "" SIGABRT
trap "" SIGFPE
trap "" SIGSEGV

EXIT=0
MISSING_FEATURE="WARNING: Missing required feature";
KNOWN_FAILURE="WARNING: Test is known to fail";

echo "Key:"
echo "ok = Test passed"
echo "n/a = Driver is missing a feature required for the test"
echo "FAIL = Unexpected failure"
echo "fail = Test failed, but it was an expected failure"
echo "PASS! = Unexpected pass"
echo ""

get_status()
{
  case $1 in
      # Special value we use to indicate that the test failed
      # but it was an expected failure so don't fail the
      # overall test run as a result...
      300)
      echo -n "fail";;
      # Special value we use to indicate that the test passed
      # but we weren't expecting it to pass‽
      400)
      echo -n 'PASS!';;

      # Special value to indicate the test is missing a required feature
      500)
      echo -n "n/a";;

      0)
      echo -n "ok";;

      *)
      echo -n "FAIL";;
  esac
}

run_test()
{
  $($TEST_BINARY $1 &>.log)
  TMP=$?
  var_name=$2_result
  eval $var_name=$TMP
  if grep -q "$MISSING_FEATURE" .log; then
    if test $TMP -ne 0; then
      eval $var_name=500
    else
      eval $var_name=400
    fi
  elif grep -q "$KNOWN_FAILURE" .log; then
    if test $TMP -ne 0; then
      eval $var_name=300
    else
      eval $var_name=400
    fi
  else
    if test $TMP -ne 0; then EXIT=$TMP; fi
  fi
}

TITLE_FORMAT="%35s"
printf $TITLE_FORMAT "Test"

if test $HAVE_GL -eq 1; then
  GL_FORMAT=" %6s %8s %7s %6s %6s"
  printf "$GL_FORMAT" "GL+FF" "GL+ARBFP" "GL+GLSL" "GL-NPT" "GL3"
fi
if test $HAVE_GLES2 -eq 1; then
  GLES2_FORMAT=" %6s %7s"
  printf "$GLES2_FORMAT" "ES2" "ES2-NPT"
fi

echo ""
echo ""

for test in `cat unit-tests`
do
  export COGL_DEBUG=

  if test $HAVE_GL -eq 1; then
    export COGL_DRIVER=gl
    export COGL_DEBUG=disable-glsl,disable-arbfp
    run_test $test gl_ff

    export COGL_DRIVER=gl
    # NB: we can't explicitly disable fixed + glsl in this case since
    # the arbfp code only supports fragment processing so we need either
    # the fixed or glsl vertends
    export COGL_DEBUG=
    run_test $test gl_arbfp

    export COGL_DRIVER=gl
    export COGL_DEBUG=disable-fixed,disable-arbfp
    run_test $test gl_glsl

    export COGL_DRIVER=gl
    export COGL_DEBUG=disable-npot-textures
    run_test $test gl_npot

    export COGL_DRIVER=gl3
    export COGL_DEBUG=
    run_test $test gl3
  fi

  if test $HAVE_GLES2 -eq 1; then
    export COGL_DRIVER=gles2
    export COGL_DEBUG=
    run_test $test gles2

    export COGL_DRIVER=gles2
    export COGL_DEBUG=disable-npot-textures
    run_test $test gles2_npot
  fi

  printf $TITLE_FORMAT "$test:"
  if test $HAVE_GL -eq 1; then
    printf "$GL_FORMAT" \
      "`get_status $gl_ff_result`" \
      "`get_status $gl_arbfp_result`" \
      "`get_status $gl_glsl_result`" \
      "`get_status $gl_npot_result`" \
      "`get_status $gl3_result`"
  fi
  if test $HAVE_GLES2 -eq 1; then
    printf "$GLES2_FORMAT" \
      "`get_status $gles2_result`" \
      "`get_status $gles2_npot_result`"
  fi
  echo ""
done

exit $EXIT
