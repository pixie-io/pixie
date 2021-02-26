#!/bin/bash
set -e

SCRIPTPATH="$( cd "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"
uipath="${SCRIPTPATH}/../src/ui"
eslint=./node_modules/.bin/eslint

cd "${uipath}"
if [[ ! -x "${eslint}" ]]; then
    yarn install
fi


"${eslint}" "$@"
