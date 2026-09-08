#!/bin/bash

# this just launches the Gestaltor

set -e

script_dir="$( dirname "$0" )"
cd $script_dir/bin

LD_LIBRARY_PATH=. ./Gestaltor "$@"
