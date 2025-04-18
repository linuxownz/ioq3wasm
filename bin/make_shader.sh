#!/bin/bash

set -eu

gitroot=$(git rev-parse --show-toplevel)

if [[ $gitroot == "" ]]; then
    echo "failed to get toplevel"
    exit 1
fi

INPUT=$gitroot/code/renderergl2/glsl/*.glsl
OUTPUT=$gitroot/code/renderergl2/tr_fallback_shader.c

rm -f $OUTPUT

for f in $INPUT; do
    filename=$(basename $f)
    varname="fallbackShader_${filename}"
    varname=${varname/.glsl/}
    # echo "filename: $filename  varname: $varname"
    xxd -i -n $varname $f >> $OUTPUT
done

