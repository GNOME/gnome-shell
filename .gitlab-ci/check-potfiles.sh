#!/usr/bin/env bash

srcdirs="src subprojects/extensions-tool"
uidirs="js subprojects/extensions-app"
desktopdirs="data subprojects/extensions-app/ subprojects/extensions-tool"

# find source files that contain gettext keywords
files=$(grep -lR --include='*.c' '\(gettext\|[^I_)]_\)(' $srcdirs)

# find ui files that contain translatable string
files="$files "$(grep -lRi --include='*.ui' 'translatable="[ty1]' $uidirs)

# find .desktop files
files="$files "$(find $desktopdirs -name '*.desktop*')

# filter out excluded files
if [ -f po/POTFILES.skip ]; then
  files=$(for f in $files; do ! grep -q ^$f po/POTFILES.skip && echo $f; done)
fi

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
