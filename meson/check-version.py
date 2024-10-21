#!/usr/bin/env python3

import os, sys
from pathlib import Path
from xml.etree.ElementTree import ElementTree
import argparse

def check_version(version, file, type='news'):
    if type == 'news':
        line = file.open().readline()
        ok = line.startswith(version)
        print("{}: {}".format(file, "OK" if ok else "FAILED"))
        if not ok:
            raise Exception("{} does not start with {}".format(file, version))
    elif type == 'metainfo':
        query = './releases/release[@version="{}"]'.format(version)
        ok = ElementTree(file=file).find(query) is not None
        print("{}: {}".format(file, "OK" if ok else "FAILED"))
        if not ok:
            raise Exception("{} does not contain release {}".format(file, version))
    else:
        raise Exception('Not implemented')

parser = argparse.ArgumentParser(description='Check release version information.')
parser.add_argument('--type', choices=['metainfo','news'], default='news')
parser.add_argument('version', help='the version to check for')
parser.add_argument('files', nargs='+', help='files to check')
args = parser.parse_args()

distroot = os.environ.get('MESON_DIST_ROOT', './')

try:
    for file in args.files:
        check_version(args.version, Path(distroot, file), args.type)
except:
    sys.exit(1)
