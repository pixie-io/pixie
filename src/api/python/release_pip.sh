#!/bin/bash -e

BUILDDIR=/tmp/blahdyblah
PKGDIR=/tmp/out
workspace=$(bazel info workspace 2> /dev/null)

# Clean up build dir
rm -rf "${BUILDDIR}"
rm -rf "${PKGDIR}"
# Go to TOT
cd "${workspace}"
bazel build //src/api/python:build_pip_package

bazel-bin/src/api/python/build_pip_package --src "${BUILDDIR}" --dest "${PKGDIR}"


# uncomment to upload to main pypi
# twine upload ${PKGDIR}/*

# Upload to test pypi
twine upload --repository testpypi ${PKGDIR}/*

