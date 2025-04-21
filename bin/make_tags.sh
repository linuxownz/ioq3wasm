#!/bin/bash

set -eu

cd $(git rev-parse --show-toplevel)/

client_files=$(make -sf Makefile.client listsourcefiles listheaderfiles | xargs | sed -e 's/code\///g')
server_files=$(make -sf Makefile.server listsourcefiles listheaderfiles | xargs | sed -e 's/code\///g')

cd $(git rev-parse --show-toplevel)/code

[ -e client_tags ] && rm client_tags
[ -e server_tags ] && rm server_tags

extra="--kinds-C=+p"

ctags $extra $client_files
mv tags client_tags

ctags $extra $server_files
mv tags server_tags


