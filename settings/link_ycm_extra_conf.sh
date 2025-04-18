#!/bin/bash

cd $(git rev-parse --show-toplevel)
cwd=$(pwd)

find code -type d -exec ln -s $cwd/settings/ycm_extra_conf.py {}/.ycm_extra_conf.py \;


