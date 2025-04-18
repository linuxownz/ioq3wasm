#!/bin/bash

set -eu

cd $(git rev-parse --show-toplevel)/code

# remove previous db
[ -e cscope.out ] && rm cscope.out

# find client C files
find ./botlib ./cgame ./client ./game ./qcommon ./renderercommon ./renderergl2 ./sdl ./sys  -name '*.c' | grep -Ev 'server\.c|snd_codec_ogg' > cscope.files

# build db
cscope -q -b -c -i cscope.files

