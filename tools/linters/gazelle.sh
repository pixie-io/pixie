#!/bin/bash

tmpdir=$(mktemp -d)
patchFile="${tmpdir}/diff.patch"
builddir=$(dirname "$1")

if bazel run //:gazelle --noshow_progress -- --mode=diff "$builddir" 2>/dev/null 1>"${patchFile}"; then
  exit 0
else
  # Here be dragons, beware all who step here.
  # gazelle responds with a diff in patch format, however afaik
  # for some reason arcanist still doesn't accept patch formats.
  # arcanist wants patches as the set of 4 pieces of info:
  # {line, char, original_text, replacement_text}
  # So we just patch the entire file, and hand arcanist the original, and new contents
  # with line, char set to 1, 1.

  fileToPatch=$(grep '^---' "$patchFile" | perl -pe 's/--- (.*)\t.*/\1/g')
  correctedFileContents=$(patch "$fileToPatch" "$patchFile" -o- 2>/dev/null)
  originalFileContents=$(cat "$fileToPatch")

  # Careful, do not modify this output without also modifyng the matcher in .arclint
  # The arcanist linter regex is sensitive to exact text and newlines.
  echo "error" # severity
  echo "$fileToPatch" # file
  echo "Gazelle was not run on file" # message
  echo "1,1"  # line,char
  echo "<<<<<"
  echo "$originalFileContents" # original
  echo "====="
  echo "$correctedFileContents" # replacement
  echo ">>>>>"
fi
