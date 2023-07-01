#!/usr/bin/bash

fetch() {
  local remote=$1
  local ref=$2

  git fetch --quiet --depth=1 $remote $ref 2>/dev/null
}

mutter_target=

echo -n Cloning into mutter ...
if git clone --quiet --depth=1 https://gitlab.gnome.org/GNOME/mutter.git; then
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
  if fetch $merge_request_remote $merge_request_branch; then
    echo \ found
    mutter_target=FETCH_HEAD
  else
    echo \ not found

    echo -n Looking for $CI_MERGE_REQUEST_TARGET_BRANCH_NAME instead ...
    if fetch origin $CI_MERGE_REQUEST_TARGET_BRANCH_NAME; then
      echo \ found
      mutter_target=FETCH_HEAD
    else
      echo \ not found
    fi
  fi
fi

if [ -z "$mutter_target" ]; then
  ref_remote=${CI_PROJECT_URL//gnome-shell/mutter}
  echo -n Looking for $CI_COMMIT_REF_NAME on remote ...
  if fetch $ref_remote $CI_COMMIT_REF_NAME; then
    echo \ found
    mutter_target=FETCH_HEAD
  else
    echo \ not found
  fi
fi

fallback_branch=${CI_COMMIT_TAG:+gnome-}${CI_COMMIT_TAG%%.*}
if [ -z "$mutter_target" -a "$fallback_branch" ]; then
  echo -n Looking for $fallback_branch instead ...
  if fetch origin $fallback_branch; then
    echo \ found
    mutter_target=FETCH_HEAD
  else
    echo \ not found
  fi
fi

if [ -z "$mutter_target" ]; then
  mutter_target=HEAD
  echo Using $mutter_target instead
fi

git checkout -q $mutter_target
