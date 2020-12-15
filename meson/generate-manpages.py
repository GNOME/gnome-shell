#!/usr/bin/env python3

import os
from pathlib import PurePath
from asciidocapi import AsciiDocAPI

man_pages = [
    'man/gnome-shell.1',
    'subprojects/extensions-tool/man/gnome-extensions.1',
]

sourceroot = os.environ.get('MESON_SOURCE_ROOT')
distroot = os.environ.get('MESON_DIST_ROOT')

asciidoc = AsciiDocAPI()

for man_page in man_pages:
    page_path = PurePath(man_page)
    src = PurePath(sourceroot, page_path.with_suffix('.txt'))
    dst = PurePath(distroot, page_path)
    stylesheet = src.with_name('stylesheet.xsl')

    asciidoc.options('--xsl-file', os.fspath(stylesheet))
    asciidoc.execute(os.fspath(src), outfile=os.fspath(dst))
