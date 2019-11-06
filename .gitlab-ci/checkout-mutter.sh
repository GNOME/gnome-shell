#!/usr/bin/bash

mutter_target=

git clone https://gitlab.gnome.org/GNOME/mutter.git

if [ $? -ne 0 ]; then
  echo Checkout failed
  exit 1
fi

cd mutter

if [ "$CI_MERGE_REQUEST_TARGET_BRANCH_NAME" ]; then
  merge_request_remote=${CI_MERGE_REQUEST_SOURCE_PROJECT_URL//gnome-shell/mutter}
  merge_request_branch=$CI_MERGE_REQUEST_SOURCE_BRANCH_NAME

  echo Looking for $merge_request_branch on remote ...
  if git fetch -q $merge_request_remote $merge_request_branch 2>/dev/null; then
    mutter_target=FETCH_HEAD
  else
    mutter_target=origin/$CI_MERGE_REQUEST_TARGET_BRANCH_NAME
    echo Using $mutter_target instead
  fi
fi

if [ -z "$mutter_target" ]; then
  mutter_target=$(git branch -r -l origin/$CI_COMMIT_REF_NAME)
  mutter_target=${mutter_target:-origin/master}
  echo Using $mutter_target instead
fi

git checkout -q $mutter_target
