#!/bin/bash
set -e

# This script is invoked with a line like:
# linters/eslint_ui.sh '--format=json' --no-color --config .eslintrc.json src/ui/.../something.ts
# That assumes that there's a .eslintrc.json in the working directory. We need the nearest matching ancestor directory.

scriptDir="$( cd "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"
uiDir="${scriptDir}/../src/ui"

# Walk up the lint target's directory ancestry until finding .eslintrc.json, or until reaching the UI root or /.
workDir=$(dirname "$5")
while [ ! "$workDir" = '/' ] && [ ! "$workDir" = "$uiDir" ] && [ ! -f "${workDir}/.eslintrc.json" ]; do
  workDir=$(dirname "$workDir")
done

if [ ! -f "${workDir}/.eslintrc.json" ]; then
  echo "Could not find .eslintrc when walking up the directory tree from $(dirname "$5")" >&2
  echo "Last location checked was $workDir" >&2
  exit 1
fi

pushd "${workDir}" > /dev/null
"${uiDir}/node_modules/.bin/eslint" "$@"
popd > /dev/null
