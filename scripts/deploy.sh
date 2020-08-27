#!/usr/bin/env bash

set -e
set -x

pushd pxl_scripts
make update_bundle
popd
