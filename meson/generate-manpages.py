#!/usr/bin/env python3

import os
from pathlib import PurePath
import subprocess

man_pages = [
    'man/gnome-shell.1',
    'subprojects/extensions-tool/man/gnome-extensions.1',
]

sourceroot = os.environ.get('MESON_SOURCE_ROOT')
distroot = os.environ.get('MESON_DIST_ROOT')

for man_page in man_pages:
    page_path = PurePath(man_page)
    src = PurePath(sourceroot, page_path.with_suffix('.txt'))
    dst = PurePath(distroot, page_path)
    stylesheet = src.with_name('stylesheet.xsl')

    subprocess.call(['a2x', '--xsl-file', os.fspath(stylesheet),
      '--format', 'manpage', '--destination-dir', os.fspath(dst.parent),
      os.fspath(src)])
