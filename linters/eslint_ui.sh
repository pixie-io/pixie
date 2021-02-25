#!/bin/bash
set -e

while getopts "v" opt; do
  case ${opt} in
    v ) echo "v6.8.0"; exit 0;
      ;;
  esac
done

SCRIPTPATH="$( cd "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"
uipath="${SCRIPTPATH}/../src/ui"
eslint=./node_modules/.bin/eslint

cd "${uipath}"
if [[ ! -x "${eslint}" ]]; then
    yarn install
fi


"${eslint}" "$@"
