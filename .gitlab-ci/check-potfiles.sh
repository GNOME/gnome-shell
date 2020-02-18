#!/usr/bin/env bash

srcdirs="js src subprojects/extensions-tool"
globs=('*.js' '*.c')

# find source files that contain gettext keywords
files=$(grep -lR ${globs[@]/#/--include=} '\(gettext\|[^I_)]_\)(' $srcdirs)

# find those that aren't listed in POTFILES.in
missing=$(for f in $files; do ! grep -q ^$f po/POTFILES.in && echo $f; done)

if [ ${#missing} -eq 0 ]; then
  exit 0
fi

cat >&2 <<EOT

The following files are missing from po/POTFILES.po:

EOT
for f in $missing; do
  echo "  $f" >&2
done
echo >&2

exit 1
