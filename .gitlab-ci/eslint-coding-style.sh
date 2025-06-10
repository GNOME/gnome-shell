#!/bin/bash

SRCDIR=$(realpath $(dirname $0)/..)
OUTDIR=$(mktemp --directory --tmpdir=$SRCDIR)
trap "rm -rf $OUTDIR" EXIT

# Turn ```javascript``` code snippets in the
# style guide into .js files in $OUTDIR
cat <<'EOF' | python3 - docs/js-coding-style.md $OUTDIR
import sys
import re

def extract_js_snippets(input_file, output_dir):
    with open(input_file, 'r') as file:
        content = file.read()

    # Find all JavaScript code blocks using regex
    js_blocks = re.findall(r'```javascript\n(.*?)\n?```', content, flags=re.DOTALL)

    for i, (match) in enumerate(js_blocks):
        js_code = match

        # Remove one level of indent
        js_code = re.sub(r'^ {4}', '', js_code, flags=re.MULTILINE)

        # The following are class snippets, turn them
        # into functions to not confuse eslint
        js_code = re.sub(r'^moveActor', 'function moveActor', js_code)
        js_code = re.sub(r'^desaturateActor', 'function desaturateActor', js_code)

        # Finally, create a .js file in the output directory
        output_filename = f'{output_dir}/{i}.js'
        with open(output_filename, 'w') as out_file:
            out_file.write(f'{js_code}\n')

input_file, output_dir = sys.argv[1:]
extract_js_snippets(input_file, output_dir)
EOF

echo Checking coding style of coding style docs
tools/run-eslint.sh "$@" $OUTDIR
