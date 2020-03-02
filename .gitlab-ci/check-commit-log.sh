#!/usr/bin/env bash

if [ -z "$CI_MERGE_REQUEST_TARGET_BRANCH_NAME" ]; then
  echo Cannot review non-merge request
  exit 1
fi

git fetch $CI_MERGE_REQUEST_PROJECT_URL.git $CI_MERGE_REQUEST_TARGET_BRANCH_NAME

branch_point=$(git merge-base HEAD FETCH_HEAD)

commits=$(git log --format='format:%H' $branch_point..$CI_COMMIT_SHA)

if [ -z "$commits" ]; then
  echo Commit range empty
  exit 1
fi

function commit_message_has_url() {
  commit=$1
  commit_message=$(git show -s --format='format:%b' $commit)
  echo "$commit_message" | grep -qe "\($CI_MERGE_REQUEST_PROJECT_URL/\(-/\)\?\(issues\|merge_requests\)/[0-9]\+\|https://bugzilla.gnome.org/show_bug.cgi?id=[0-9]\+\)"
  return $?
}

for commit in $commits; do
  if ! commit_message_has_url $commit; then
    echo "Missing merge request or issue URL on commit $(echo $commit | cut -c -8)"
    exit 1
  fi
done
