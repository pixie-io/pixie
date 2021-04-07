#!/bin/bash -e

# Increment this number on every upload.
version=1.1
tag="gcr.io/pl-dev-infra/python_mysql_connector:$version"

docker build . -t $tag
docker push $tag


sha=$(docker inspect --format='{{index .RepoDigests 0}}' $tag | cut -f2 -d'@')

echo ""
echo "Image pushed!"
echo "IMPORTANT: Now update //bazel/pl_workspace.bzl with the following digest: $sha"

