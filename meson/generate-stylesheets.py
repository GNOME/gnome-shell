#!/usr/bin/env python3

import os
from pathlib import PurePath
import subprocess

stylesheets = [
    'data/theme/gnome-shell-high-contrast.css',
    'data/theme/gnome-shell.css'
]

sourceroot = os.environ.get('MESON_SOURCE_ROOT')
distroot = os.environ.get('MESON_DIST_ROOT')

for stylesheet in stylesheets:
    stylesheet_path = PurePath(stylesheet)
    src = PurePath(sourceroot, stylesheet_path.with_suffix('.scss'))
    dst = PurePath(distroot, stylesheet_path)
    subprocess.run(['sassc', '-a', src, dst], check=True)
