#!/bin/bash

# Copyright 2018- The Pixie Authors.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# SPDX-License-Identifier: Apache-2.0

set -e

trap exit INT

if [ "$#" -lt 3 ]; then
  echo "Usage: build_tar_for_packages.sh <architecture> <output_tar_path> <package_group_file>..."
  echo -e "\t This script downloads all of the debs it finds in each package_group_file, and extracts them into a single 'sysroot'"
  echo -e "\t The 'sysroot' is then tar'd and output at <output_tar_path>"
  exit 1
fi

arch="$1"
output_tar_path="$(realpath "$2")"

apt_conf="/conf/${arch}.apt.conf"

# Update the apt cache for the current arch.
apt-get -c "${apt_conf}" update &> /dev/null

packages=()
for file in "${@:3}"
do
  if [ -f "${file}" ]; then
    readarray -t -O "${#packages[@]}" packages < <(grep -v "^#" "${file}")
  else
    echo "Cannot find package file ${file}"
    exit 1
  fi
done

# Verify all packages exist.
packages_tmpfile="$(mktemp)"
printf "%s\n" "${packages[@]}" | sort > "${packages_tmpfile}"
missing_packages="$(apt-cache -c "${apt_conf}" pkgnames | sort | comm -13 - "${packages_tmpfile}")"
if [ -n "${missing_packages}" ]; then
  echo "Cannot find packages: ${missing_packages}"
  exit 1
fi
rm "${packages_tmpfile}"


declare -A dependency_transitive_closure

add_to_set() {
  local pkg="$1"
  if [[ "${pkg}" != *":"* ]]; then
    pkg="${pkg}:${arch}"
  fi
  if [ -z "${dependency_transitive_closure[${pkg}]}" ]; then
    dependency_transitive_closure["${pkg}"]="1"
  fi
}
remove_from_set() {
  local pkg="$1"
  if [[ "${pkg}" != *":"* ]]; then
    pkg="${pkg}:${arch}"
  fi
  unset dependency_transitive_closure["${pkg}"]
}

get_deps() {
  apt-cache -c "${apt_conf}" depends --recurse --no-recommends --no-suggests --no-conflicts \
    --no-breaks --no-replaces --no-enhances "$@" | grep "^\w"
}

get_virtual_packages() {
  apt -c "${apt_conf}" show "$@" 2>/dev/null | grep -B 1 "not a real package" | grep -Po "(?<=Package: )(.*)"
}


# Add arch suffix to each package.
for i in "${!packages[@]}"
do
  packages[$i]="${packages[$i]}:${arch}"
done

# Add dependencies that don't exist to the set.
while read -r dependency; do
    add_to_set "${dependency}"
done < <(get_deps "${packages[@]}")

# Remove virtual packages from the dependency set.
while read -r virtual_pkg; do
  echo "Skipping virtual package: ${virtual_pkg}"
  remove_from_set "${virtual_pkg}"
done < <(get_virtual_packages "${!dependency_transitive_closure[@]}")

echo "Dependencies to be added to archive:"
for pkg in "${!dependency_transitive_closure[@]}"
do
  echo "- ${pkg}"
done

relativize_symlinks() {
  dir="$1"
  libdirs=("lib" "lib64" "usr/lib")
  pushd "${dir}" > /dev/null

  while read -r link target; do
    # Skip links targeting non-absolute paths.
    if [[ "${target}" != "/"* ]]; then
      continue
    fi
    # Remove all non-"/" characters from the link name. Then replace each "/" with "../".
    prefix=$(echo "${link}" | sed -e 's|[^/]||g' | sed -e 's|/|../|g')
    ln -snf "${prefix}${target}" "${link}"
  done < <(find "${libdirs[@]}" -type l -printf '%p %l\n')

  popd > /dev/null
}

inside_tmpdir() {
  apt-get -c "${apt_conf}" download "${!dependency_transitive_closure[@]}" &>/dev/null
  root_dir="root"
  while read -r deb; do
    dpkg-deb -x "${deb}" "${root_dir}" &>/dev/null
  done < <(ls -- *.deb)

  relativize_symlinks "${root_dir}"

  tar -C "${root_dir}" -czf "${output_tar_path}" .
}

tmpdir="$(mktemp -d)"
pushd "${tmpdir}" > /dev/null

inside_tmpdir

popd > /dev/null
rm -rf "${tmpdir}"
