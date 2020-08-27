#!/usr/bin/env bash

set -e

printenv

pushd pxl_scripts
make
popd
