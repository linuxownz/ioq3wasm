#!/bin/bash

set -eu

cd $(git rev-parse --show-toplevel)/code

extra="--kinds-C=+p"

ctags -R $extra botlib cgame client game qcommon renderercommon renderergl2 sdl server sys wsServer $EMSDK/upstream/emscripten/cache/sysroot/include/

