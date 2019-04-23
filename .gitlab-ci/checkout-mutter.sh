#!/usr/bin/bash

shell_branch=$(git describe --contains --all HEAD)
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
  if git fetch -q $merge_request_remote $merge_request_branch; then
    mutter_target=FETCH_HEAD
  fi
fi

if [ -z "$mutter_target" ]; then
  mutter_target=$(git branch -r -l $shell_branch)
  mutter_target=${mutter_target:-origin/master}
  echo Using $mutter_target instead
fi

git checkout -q $mutter_target
