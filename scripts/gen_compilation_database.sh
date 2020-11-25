#!/bin/bash

RELEASE_VERSION=0.3.5

workspace=$(bazel info workspace 2> /dev/null)
pushd "${workspace}" || exit

if [[ ! -d bazel-compilation-database-${RELEASE_VERSION} ]]; then
  curl -L https://github.com/grailbio/bazel-compilation-database/archive/${RELEASE_VERSION}.tar.gz | tar -xz
  pushd bazel-compilation-database-${RELEASE_VERSION} || exit
  patch -p1 < "${workspace}/third_party/bazel_compilation_database.patch"
  popd || exit
fi

bazel-compilation-database-${RELEASE_VERSION}/generate.sh "$@"

popd || return