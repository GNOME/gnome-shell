#!/usr/bin/bash

# We need a coverity token to fetch the tarball
if [ -x $COVERITY_TOKEN ]
then
  echo "No coverity token. Run this job from a protected branch."
  exit -1
fi

mkdir -p coverity

# Download and check MD5 first
curl https://scan.coverity.com/download/linux64 \
  --data "token=$COVERITY_TOKEN&project=GNOME+Shell&md5=1" \
  --output /tmp/coverity_tool.md5

diff /tmp/coverity_tool.md5 coverity/coverity_tool.md5 >/dev/null 2>&1

if [ $? -eq 0 -a -d coverity/cov-analysis* ]
then
  echo "Coverity tarball is up-to-date"
  exit 0
fi

# Download and extract coverity tarball
curl https://scan.coverity.com/download/linux64 \
  --data "token=$COVERITY_TOKEN&project=GNOME+Shell" \
  --output /tmp/coverity_tool.tgz

rm -rf ./coverity/cov-analysis*

tar zxf /tmp/coverity_tool.tgz -C coverity/
if [ $? -eq 0 ]
then
  mv /tmp/coverity_tool.md5 coverity/
fi

rm /tmp/coverity_tool.tgz
