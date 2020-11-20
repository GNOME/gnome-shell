#!/usr/bin/env bash

if [ -z "$CI_MERGE_REQUEST_TARGET_BRANCH_NAME" ]; then
  echo This is not a merge request, skipping
  exit 0
fi

git fetch $CI_MERGE_REQUEST_PROJECT_URL.git $CI_MERGE_REQUEST_TARGET_BRANCH_NAME

branch_point=$(git merge-base HEAD FETCH_HEAD)

commits=$(git log --format='format:%H' $branch_point..$CI_COMMIT_SHA)

if [ -z "$commits" ]; then
  echo Commit range empty
  exit 1
fi

JUNIT_REPORT_TESTS_FILE=$(mktemp)

function append_failed_test_case() {
  test_name="$1"
  commit="$2"
  test_message="$3"
  commit_short=${commit:0:8}

  echo "<testcase name=\"$test_name: $commit_short\"><failure message=\"$commit_short: $test_message\"/></testcase>" >> $JUNIT_REPORT_TESTS_FILE
  echo &>2 "Commit check failed: $commit_short: $test_message"
}

function append_passed_test_case() {
  test_name="$1"
  commit="$2"
  commit_short=${commit:0:8}
  echo "<testcase name=\"$test_name: $commit_short\"></testcase>" >> $JUNIT_REPORT_TESTS_FILE
}

function generate_junit_report() {
  junit_report_file="$1"
  num_tests=$(cat "$JUNIT_REPORT_TESTS_FILE" | wc -l)
  num_failures=$(grep '<failure ' "$JUNIT_REPORT_TESTS_FILE" | wc -l )

  echo Generating JUnit report \"$(pwd)/$junit_report_file\" with $num_tests tests and $num_failures failures.

  cat > $junit_report_file << __EOF__
<?xml version="1.0" encoding="utf-8"?>
<testsuites tests="$num_tests" errors="0" failures="$num_failures">
<testsuite name="commit-review" tests="$num_tests" errors="0" failures="$num_failures" skipped="0">
$(< $JUNIT_REPORT_TESTS_FILE)
</testsuite>
</testsuites>
__EOF__
}

function commit_message_has_mr_url() {
  commit=$1
  commit_message=$(git show -s --format='format:%b' $commit)
  echo "$commit_message" | grep -qe "^$CI_MERGE_REQUEST_PROJECT_URL\/\(-\/\)\?merge_requests\/$CI_MERGE_REQUEST_IID$"
  return $?
}

for commit in $commits; do
  if commit_message_has_mr_url $commit; then
    append_failed_test_case superfluous_url $commit \
      "Commit message must not contain a link to its own merge request"
  else
    append_passed_test_case superfluous_url $commit
  fi
done

generate_junit_report commit-message-junit-report.xml

! grep -q '<failure' commit-message-junit-report.xml
exit $?
