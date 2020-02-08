#!/usr/bin/env bash

set -e

eslint_exe=node_modules/.bin/eslint
if [ ! -f "$eslint_exe" ]; then
    yarn install
fi


"$eslint_exe" src ./*.js
