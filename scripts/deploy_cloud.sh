#!/bin/bash -ex

# This deploys a new cloud release.

usage() {
    echo "Usage: echo <changelog message> | $0 <artifact_type> [-r]"
    echo " -r : Create a prod release."
    echo "Example: echo 'this is a cloud prod release' | $0 cloud -r"
}

parse_args() {
  if [ $# -lt 1 ]; then
    usage
  fi

  ARTIFACT_TYPE=$1
  shift

  while test $# -gt 0; do
      case "$1" in
        -r) RELEASE=true
            shift
            ;;
        *)  usage ;;
      esac
  done
}

check_args() {
    if [ "$ARTIFACT_TYPE" != "cloud" ] && [ "$ARTIFACT_TYPE" != "docs" ]; then
        echo "Unsupported artifact type."
        exit
    fi
}

parse_args "$@"
check_args

# Get input from stdin.
CHANGELOG=''
while IFS= read -r line; do
    CHANGELOG="${CHANGELOG}${line}\n"
done

timestamp="$(date +"%s")"
release="staging"
if [ "$RELEASE" = "true" ]; then
  release="prod"
fi

new_tag="release/$ARTIFACT_TYPE/$release/$timestamp"
git tag -a "$new_tag" -m "$(echo -e "$CHANGELOG")"

git push origin "$new_tag"
