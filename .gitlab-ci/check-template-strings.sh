#!/usr/bin/env bash

# find files from POTFILES.in that use js template strings
baddies=$(grep -l '${' $(grep ^js po/POTFILES.in))

if [ ${#baddies} -eq 0 ]; then
  exit 0
fi

cat >&2 <<EOT

xgettext cannot handle template strings properly, so we ban their use
in files with translatable strings.

The following files are listed in po/POTFILES.in and use template strings:

EOT
for f in $baddies; do
  echo "  $f" >&2
done
echo >&2

exit 1
