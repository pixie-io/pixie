#!/usr/bin/env bash

set -e

git rev-parse HEAD > GIT_COMMIT

if [ -n "${TAG_NAME}" ]; then
    # Valid tag is present so we write it to the versions file.
    echo "${TAG_NAME##*/v}" > VERSION
else
    # No valid tag, treat it as a dev build.
    # We automatically set the last digit to the build number.
    echo "0.0.${BUILD_NUMBER}-${JOB_NAME}-dev" > VERSION
fi

git diff -U0 origin/main > diff_origin_main
git diff -U0 origin/main -- '***.cc' '***.h' '***.c' > diff_origin_main_cc

git diff -U0 HEAD~10 > diff_head
git diff -U0 HEAD~10 -- '***.cc' '***.h' '***.c' > diff_head_cc
