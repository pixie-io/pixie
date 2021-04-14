#!/bin/bash -e

builddir=$(dirname "$1")

bad_targets=$(bazel query "$builddir/... except attr('tags', 'manual', $builddir/...)" --noshow_progress 2>/dev/null)

for target in $bad_targets; do
  echo "$1: Experimental build target $target should be marked with tag \"manual\""
done