#!/usr/bin/bash

mutter_target=

echo -n Cloning into mutter ...
if git clone --quiet https://gitlab.gnome.org/GNOME/mutter.git; then
  echo \ done
else
  echo \ failed
  exit 1
fi

cd mutter

if [ "$CI_MERGE_REQUEST_TARGET_BRANCH_NAME" ]; then
  merge_request_remote=${CI_MERGE_REQUEST_SOURCE_PROJECT_URL//gnome-shell/mutter}
  merge_request_branch=$CI_MERGE_REQUEST_SOURCE_BRANCH_NAME

  echo -n Looking for $merge_request_branch on remote ...
  if git fetch -q $merge_request_remote $merge_request_branch 2>/dev/null; then
    echo \ found
    mutter_target=FETCH_HEAD
  else
    echo \ not found
    mutter_target=origin/$CI_MERGE_REQUEST_TARGET_BRANCH_NAME
    echo Using $mutter_target instead
  fi
fi

if [ -z "$mutter_target" ]; then
  echo -n Looking for $CI_COMMIT_REF on remote ...
  mutter_target=$(git branch -r -l origin/$CI_COMMIT_REF_NAME)
  if [ "$mutter_target" ]; then
    echo \ found
  else
    echo \ not found
    mutter_target=${mutter_target:-origin/master}
    echo Using $mutter_target instead
  fi
fi

git checkout -q $mutter_target
