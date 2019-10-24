#!/bin/bash -ex

# This script gets the number of commits between the given release tag and the previous tag.

if [ $# -lt 2 ]; then
    echo "Usage: $0 <release-tag> <bazel-target>"
    exit
fi

tag_name=$1
bazel_target=$2

# Get the release string from the provided tag name (release/cli or release/vizier).
release_tag=${tag_name//-*/}

# Get all matching release tags.
tags=$(git for-each-ref --sort='-*authordate' --format '%(refname:short)' refs/tags | grep "$release_tag")

# Find the tag before the given tag.
prev_tag=$(echo "$tags" | grep -A 1 "$tag_name" | tail -1)

# Find all commits between the two tags.
commits=$(git log "$tag_name"..."$prev_tag" --pretty=format:"%H")

# Find all file dependencies of the bazel target.
bazel query 'kind("source file", deps('"$bazel_target"'))' | sed  -e 's/:/\//' -e 's/^\/\+//' > target_files.txt
trap "rm -f target_files.txt" EXIT

commit_count=0
# For each commit, check if it has modified a file in the bazel target's dependencies.
for commit in $commits
do
    files=$(git show --name-only --format=oneline "$commit" | tail -n +2)
    for file in $files
    do
        if grep -iq "$file" target_files.txt; then
            commit_count=$((commit_count + 1))
            break
        fi
    done
done

echo $commit_count
