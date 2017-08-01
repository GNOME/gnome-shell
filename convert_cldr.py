#!/usr/bin/python

from gi.repository import Gdk
from xml.etree.ElementTree import ElementTree, Element
import re

ESCAPE_PATTERN = re.compile(r'\\u\{([0-9A-Fa-f]+?)\}')
ISO_PATTERN = re.compile(r'[A-E]([0-9]+)')


def parse_single_key(value):
    key = Element('key')
    uc = 0
    if hasattr(__builtins__, 'unichr'):
        def unescape(m):
            return chr(int(m.group(1), 16))
    else:
        def unescape(m):
            return chr(int(m.group(1), 16))
    value = ESCAPE_PATTERN.sub(unescape, value)
    if len(value) > 1:
        key.set('text', value)
    uc = ord(value[0])
    keyval = Gdk.unicode_to_keyval(uc)
    name = Gdk.keyval_name(keyval)
    key.set('name', name)
    return key


def convert(source, tree):
    root = Element('layout')
    for index, keymap in enumerate(tree.iter('keyMap')):
        level = Element('level')
        rows = {}
        root.append(level)
        level.set('name', 'level%d' % (index+1))
        # FIXME: heuristics here
        modifiers = keymap.get('modifiers')
        if not modifiers:
            mode = 'default'
        elif 'shift' in modifiers.split(' ') or 'lock' in modifiers.split(' '):
            mode = 'latched'
        else:
            mode = 'locked'
        level.set('mode', mode)
        for _map in keymap.iter('map'):
            value = _map.get('to')
            key = parse_single_key(value)
            iso = _map.get('iso')
            if not ISO_PATTERN.match(iso):
                sys.stderr.write('invalid ISO key name: %s\n' % iso)
                continue
            if not iso[0] in rows:
                rows[iso[0]] = []
            rows[iso[0]].append((int(iso[1:]), key))
            # add attribute to certain keys
            name = key.get('name')
            if name == 'space':
                key.set('align', 'center')
                key.set('width', '6.0')
            if name in ('space', 'BackSpace'):
                key.set('repeatable', 'yes')
            # add subkeys
            longPress = _map.get('longPress')
            if longPress:
                for value in longPress.split(' '):
                    subkey = parse_single_key(value)
                    key.append(subkey)
        for k, v in sorted(list(rows.items()), key=lambda x: x[0], reverse=True):
            row = Element('row')
            for key in sorted(v, key=lambda x: x):
                row.append(key[1])
            level.append(row)
    return root


def indent(elem, level=0):
    i = "\n" + level*"  "
    if len(elem):
        if not elem.text or not elem.text.strip():
            elem.text = i + "  "
        if not elem.tail or not elem.tail.strip():
            elem.tail = i
        for elem in elem:
            indent(elem, level+1)
        if not elem.tail or not elem.tail.strip():
            elem.tail = i
    else:
        if level and (not elem.tail or not elem.tail.strip()):
            elem.tail = i

if __name__ == "__main__":
    import sys

    if len(sys.argv) != 2:
        print("supply a CLDR keyboard file")
        sys.exit(1)

    source = sys.argv[-1]
    itree = ElementTree()
    itree.parse(source)

    root = convert(source, itree)
    indent(root)

    otree = ElementTree(root)
    if hasattr(sys.stdout, 'buffer'):
        out = sys.stdout.buffer
    else:
        out = sys.stdout
    otree.write(out, xml_declaration=True, encoding='UTF-8')
