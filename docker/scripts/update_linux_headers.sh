#!/bin/bash

# This script builds and uploads the linux kernel headers for an array of kernel versions.
# If the linux_headers_image/Dockerfile is changed in any way, or a new kernel version is added,
# this script should be run again.

script_dir="$(dirname "$0")"
cd "$script_dir"/.. || exit

# Increase this number if uploading a new set of headers.
REV=pl3

KERNEL_VERSIONS=(
  4.14.176
  4.15.18
  4.16.18
  4.17.19
  4.18.20
  4.19.118
  4.20.17
  5.0.21
  5.1.21
  5.2.21
  5.3.18
  5.4.35
)

for version in "${KERNEL_VERSIONS[@]}"; do
  echo "Building and uploading $version"
  major=${version%%.*}
  make LINUX_MAJOR_VERSION="$major" LINUX_KERNEL_VERSION="$version" LINUX_HEADERS_REV="$REV" upload_linux_headers
done

echo "-------------------------------------------------------------------------"
echo " File hashes"
echo "-------------------------------------------------------------------------"

for version in "${KERNEL_VERSIONS[@]}"; do
  filename="linux-headers-${version}-trimmed-${REV}.tar.gz"
  echo "$filename"
  curl -sL "https://storage.googleapis.com/pl-infra-dev-artifacts/${filename}" | sha256sum
done
