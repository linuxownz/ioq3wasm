#!/bin/bash

set -eu

cd $(git rev-parse --show-toplevel)/code

# remove previous dbs
[ -e cscope_client.out ] && rm cscope_client.out
[ -e cscope_server.out ] && rm cscope_server.out

# client C files
make -f ../Makefile.client listsourcefiles | sed -e 's/code\//\n/g' > cscope_client.files

# server C files
make -f ../Makefile.server listsourcefiles | sed -e 's/code\//\n/g' > cscope_server.files

# build db
cscope -q -b -c -i cscope_client.files

rm cscope.in.out cscope.po.out cscope_client.files
mv cscope.out cscope_client.out

cscope -q -b -c -i cscope_server.files
rm cscope.in.out cscope.po.out cscope_server.files
mv cscope.out cscope_server.out

