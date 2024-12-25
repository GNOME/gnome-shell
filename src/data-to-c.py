#!/usr/bin/env python3

# Copyright © 2024 Red Hat, Inc
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2 of the licence, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, see <http://www.gnu.org/licenses/>.
#
# Author: Alberto Ruiz <aruiz@redhat.com>

import sys

if len(sys.argv) != 3:
    print(f"Usage: {sys.argv[0]} <filename> <variable>", file=sys.stderr)
    sys.exit(1)

with open(sys.argv[1], "rb") as srcfile:
    print(f'const char {sys.argv[2]}[] = "', end="")
    while (val := srcfile.read(1)):
        print(f"\\x{val.hex()}", end="")
    print('";')
