#!/bin/bash

set -eu

cd $(git rev-parse --show-toplevel)/code

[ -e client_tags ] && rm client_tags
[ -e server_tags ] && rm server_tags

client_files=$(make -f ../Makefile.client listsourcefiles | sed -e 's/code\///g')
server_files=$(make -f ../Makefile.server listsourcefiles | sed -e 's/code\///g')

extra="--kinds-C=+p"

ctags -R $extra $client_files
mv tags client_tags

ctags -R $extra $server_files
mv tags server_tags


