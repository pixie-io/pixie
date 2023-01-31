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

if [ "$#" -lt 4 ]; then
  echo "Usage: build_tar_for_packages.sh <package_satisifier_path> <package_database_file> <output_tar_path> <package_group_yaml>..."
  echo -e "\t This script downloads all of the debs (along with depedencies) it finds in each package_group_yaml, and extracts them into a single 'sysroot'"
  echo -e "\t The 'sysroot' is then tar'd and output at <output_tar_path>"
  exit 1
fi

debian_mirror="${DEBIAN_MIRROR:-http://ftp.us.debian.org/debian/}"

package_satisifier_path="$(realpath "$1")"
package_database_file="$(realpath "$2")"
output_tar_path="$(realpath "$3")"
package_parser_args=("--pkgdb" "${package_database_file}")
for yaml in "${@:4}"
do
  package_parser_args+=("--specs" "${yaml}")
done

debs=()
while read -r deb; do
  debs+=("${debian_mirror}/${deb}")
done < <("${package_satisifier_path}" "${package_parser_args[@]}")

echo "Dependencies to be added to archive:"
for deb in "${debs[@]}"
do
  echo "- ${deb}"
done

declare -A paths_to_exclude
while read -r path; do
  if [ -n "${path}" ]; then
    paths_to_exclude["${path}"]=true
  fi
done < <(yq eval -N '.path_excludes[]' "${@:4}")

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
  echo "${debs[@]}" | xargs curl -fLO --remote-name-all &> /dev/null

  root_dir="root"
  while read -r deb; do
    dpkg-deb -x "${deb}" "${root_dir}" &>/dev/null
  done < <(ls -- *.deb)

  for path in "${!paths_to_exclude[@]}"
  do
    echo "Removing ${path} from sysroot"
    rm -rf "${root_dir:?}/${path:?}"
  done

  relativize_symlinks "${root_dir}"

  tar -C "${root_dir}" -czf "${output_tar_path}" .
}

tmpdir="$(mktemp -d)"
pushd "${tmpdir}" > /dev/null

inside_tmpdir

popd > /dev/null
rm -rf "${tmpdir}"
