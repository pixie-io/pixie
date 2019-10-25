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

git diff -U0 origin/master > diff_origin_master
git diff -U0 origin/master -- '***.cc' '***.h' '***.c' > diff_origin_master_cc
