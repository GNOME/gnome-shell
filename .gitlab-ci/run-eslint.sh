#!/usr/bin/env bash

OUTPUT_REGULAR=reports/lint-regular-report.txt
OUTPUT_LEGACY=reports/lint-legacy-report.txt
OUTPUT_FINAL=reports/lint-common-report.txt

OUTPUT_MR=reports/lint-mr-report.txt

LINE_CHANGES=changed-lines.txt

is_empty() {
  (! grep -q . $1)
}

run_eslint() {
  ARGS_LEGACY='--config lint/eslintrc-legacy.yml'

  local extra_args=ARGS_$1
  local output_var=OUTPUT_$1
  local output=${!output_var}
  local cache=.eslintcache-${1,,}

  # ensure output exists even if eslint doesn't report any errors
  mkdir -p $(dirname $output)
  touch $output

  eslint -f unix --cache --cache-location $cache ${!extra_args} -o $output js
}

list_commit_range_additions() {
  # Turn raw context-less git-diff into a list of
  # filename:lineno pairs of new (+) lines
  git diff -U0 "$@" -- js |
  awk '
    BEGIN { file=""; }
    /^+++ b/ { file=substr($0,7); }
    /^@@ / {
        len = split($3,a,",")
        start=a[1]
        count=(len > 1) ? a[2] : 1

        for (line=start; line<start+count; line++)
            printf "%s/%s:%d:\n",ENVIRON["PWD"],file,line;
    }'
}

copy_matched_lines() {
  local source=$1
  local matches=$2
  local target=$3

  echo -n > $target
  for l in $(<$matches); do
    grep $l $source >> $target
  done
}

create_common() {
  # comm requires sorted input;
  # we also strip the error message to make the following a "common" error:
  # regular:
  #  file.js:42:23 Indentation of 55, expected 42
  # legacy:
  #  file.js:42:23 Indentation of 55, extected 24
  prepare() {
    sed 's: .*::' $1 | sort
  }

  comm -12 <(prepare $OUTPUT_REGULAR) <(prepare $OUTPUT_LEGACY) >$OUTPUT_FINAL.tmp

  # Now add back the stripped error messages
  copy_matched_lines $OUTPUT_REGULAR $OUTPUT_FINAL.tmp $OUTPUT_FINAL
  rm $OUTPUT_FINAL.tmp
}

# Disable MR handling for now. We aren't ready to enforce
# non-legacy style just yet ...
unset CI_MERGE_REQUEST_TARGET_BRANCH_NAME

REMOTE=${1:-$CI_MERGE_REQUEST_PROJECT_URL.git}
BRANCH_NAME=${2:-$CI_MERGE_REQUEST_TARGET_BRANCH_NAME}

if [ "$BRANCH_NAME" ]; then
  git fetch $REMOTE $BRANCH_NAME
  branch_point=$(git merge-base HEAD FETCH_HEAD)
  commit_range=$branch_point...HEAD

  list_commit_range_additions $commit_range > $LINE_CHANGES

  # Don't bother with running lint when no JS changed
  if is_empty $LINE_CHANGES; then
    exit 0
  fi
fi

echo Generating lint report using regular configuration
run_eslint REGULAR
echo Generating lint report using legacy configuration
run_eslint LEGACY
echo Done.
create_common

if ! is_empty $OUTPUT_FINAL; then
  cat $OUTPUT_FINAL
  exit 1
fi

# Just show the report and succeed when not testing a MR
if [ -z "$BRANCH_NAME" ]; then
  exit 0
fi

copy_matched_lines $OUTPUT_REGULAR $LINE_CHANGES $OUTPUT_MR
cat $OUTPUT_MR
is_empty $OUTPUT_MR
