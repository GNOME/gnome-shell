#!/usr/bin/env python3

import argparse

def read_app_ids(path) -> str:
    ids = []
    with open(path, "r") as file:
        for line in file:
            # strip comments
            line, _, _ = line.partition('#');
            line = line.strip()
            if len(line) > 0:
                ids.append(line)
    return ids

def print_as_array(ids):
    mapped_ids = list(map(lambda i: f"  '{i}'", ids))
    print('[')
    print(',\n'.join(mapped_ids))
    print(']')

def print_as_pages(ids):
    mapped_ids = []
    for i, id in enumerate(ids):
        mapped_ids.append(f"  '{id}': <{{'position': <{i}>}}>")

    print('[{')
    print(',\n'.join(mapped_ids))
    print('}]')

parser = argparse.ArgumentParser()
parser.add_argument('--pages', action='store_true')
parser.add_argument('file')
args = parser.parse_args()

ids = read_app_ids(args.file)
if args.pages:
    print_as_pages(ids)
else:
    print_as_array(ids)
