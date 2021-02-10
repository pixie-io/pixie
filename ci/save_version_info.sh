#!/usr/bin/env bash

set -e

git rev-parse HEAD > GIT_COMMIT

if [[ "${TAG_NAME}" == *"/v"* ]]; then
    # Valid tag is present so we write it to the versions file.
    echo "${TAG_NAME##*/v}" > VERSION
else
    # No valid tag, treat it as a dev build.
    # We automatically set the last digit to the build number.
    
    # Semver tags can't contain extra "-"'s, so we need to remove them from the 
    # job name.
    sanitized_job_name=$(echo "${JOB_NAME}" | sed -r 's/-/\//g')
    echo "0.0.${BUILD_NUMBER}-${sanitized_job_name}-dev" > VERSION
fi

git diff -U0 origin/main > diff_origin_main
git diff -U0 origin/main -- '***.cc' '***.h' '***.c' > diff_origin_main_cc

git diff -U0 HEAD~10 > diff_head
git diff -U0 HEAD~10 -- '***.cc' '***.h' '***.c' > diff_head_cc
