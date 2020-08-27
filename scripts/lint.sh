#!/usr/bin/env bash

set -e

flake8 --config=.pxl.flake8rc pxl_scripts/px/**/*.pxl
