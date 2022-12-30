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

# This is heavily borrowed from Chrome:
#   https://chromium.googlesource.com/chromium/src/build/+/refs/heads/main/linux/sysroot_scripts/sysroot-creator.sh.

######################################################################
# Config
######################################################################
set -o nounset
set -o errexit

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [ -z "${DIST:-}" ]; then
  echo "error: DIST not defined"
  exit 1
fi
if [ -z "${KEYRING_FILE:-}" ]; then
  echo "error: KEYRING_FILE not defined"
  exit 1
fi
if [ -z "${DEBIAN_PACKAGES:-}" ]; then
  echo "error: DEBIAN_PACKAGES not defined"
  exit 1
fi
readonly REQUIRED_TOOLS="curl xzcat"

OUTPUT_FILE=
SELECTED_ARCH=amd64
######################################################################
# Package Config
######################################################################
readonly PACKAGES_EXT=xz
readonly RELEASE_FILE="Release"
readonly RELEASE_FILE_GPG="Release.gpg"
readonly DEBIAN_DEP_LIST_AMD64="generated_package_lists/${DIST}.amd64"
readonly DEBIAN_DEP_LIST_ARM64="generated_package_lists/${DIST}.arm64"


######################################################################
# Download & Package funcs.
######################################################################
download_or_copy_unique_filename() {
  # Use this function instead of download_or_copy when the url uniquely
  # identifies the file, but the filename (excluding the directory)
  # does not.
  local url="$1"
  local dest="$2"
  local hash
  hash="$(echo "$url" | sha256sum | cut -d' ' -f1)"
  download_or_copy "${url}" "${dest}.${hash}"
  # cp the file to prevent having to redownload it, but mv it to the
  # final location so that it's atomic.
  cp "${dest}.${hash}" "${dest}.$$"
  mv "${dest}.$$" "${dest}"
}
download_or_copy() {
  if [ -f "$2" ] ; then
    echo "    - Already in place"
    return
  fi
  HTTP=0
  echo "$1" | grep -Eqs '^https?://' && HTTP=1
  if [ "$HTTP" = "1" ]; then
    sub_banner "downloading from $1 -> $2"
    # Appending the "$$" shell pid is necessary here to prevent concurrent
    # instances of sysroot-creator.sh from trying to write to the same file.
    local temp_file="${2}.partial.$$"
    # curl --retry doesn't retry when the page gives a 4XX error, so we need to
    # manually rerun.
    for i in {1..10}; do
      # --create-dirs is added in case there are slashes in the filename, as can
      # happen with the "debian/security" release class.
      local http_code
      http_code=$(curl -L "$1" --create-dirs -o "${temp_file}" \
                        -w "%{http_code}")
      if [ "${http_code}" -eq 200 ]; then
        break
      fi
      echo "Bad HTTP code ${http_code} when downloading $1"
      rm -f "${temp_file}"
      sleep "$i"
    done
    if [ ! -f "${temp_file}" ]; then
      exit 1
    fi
    mv "${temp_file}" "$2"
  else
    sub_banner "copying from $1"
    cp "$1" "$2"
  fi
}

#
# check_for_debian_gpg_keyring
#
#     Make sure the Debian GPG keys exist. Otherwise print a helpful message.
#
check_for_debian_gpg_keyring() {
  if [ ! -e "$KEYRING_FILE" ]; then
    echo "KEYRING_FILE not found: ${KEYRING_FILE}"
    echo "Debian GPG keys missing. Install the debian-archive-keyring package."
    exit 1
  fi
}

#
# verify_package_listing
#
#     Verifies the downloaded Packages.xz file has the right checksums.
#
verify_package_listing() {
  local file_path="$1"
  local output_file="$2"
  local repo="$3"
  local dist="$4"
  local repo_basedir="${repo}/dists/${dist}"
  local release_list="${repo_basedir}/${RELEASE_FILE}"
  local release_list_gpg="${repo_basedir}/${RELEASE_FILE_GPG}"
  local release_file="${BUILD_DIR}/${dist}-${RELEASE_FILE}"
  local release_file_gpg="${BUILD_DIR}/${dist}-${RELEASE_FILE_GPG}"
  check_for_debian_gpg_keyring
  download_or_copy_unique_filename "${release_list}" "${release_file}"
  download_or_copy_unique_filename "${release_list_gpg}" "${release_file_gpg}"
  echo "Verifying: ${release_file} with ${release_file_gpg}"
  gpgv -q --keyring "${KEYRING_FILE}" "${release_file_gpg}" "${release_file}"
  echo "Verifying: ${output_file}"
  local sha256sum
  sha256sum=$(grep -E "${file_path}\$|:\$" "${release_file}" | \
    grep "SHA256:" -A 1 | xargs echo | awk '{print $2;}')
  if [ "${#sha256sum}" -ne "64" ]; then
    echo "Bad sha256sum from ${release_list}"
    exit 1
  fi
  echo "${sha256sum}  ${output_file}" | sha256sum --quiet -c
}

generate_package_list() {
  local input_file="$1"
  local output_file="$2"
  echo "Updating: ${output_file} from ${input_file}"
  /bin/rm -f "${output_file}"
  shift
  shift
  local failed=0
  # shellcheck disable=SC2068
  for pkg in $@ ; do
    local pkg_full
    pkg_full=$(grep -A 1 " ${pkg}\$" "$input_file" | \
      grep -E "pool/.*" | sed 's/.*Filename: //')
    if [ -z "${pkg_full}" ]; then
      echo "ERROR: missing package: $pkg"
      local failed=1
    else
      local sha256sum
      sha256sum=$(grep -A 4 " ${pkg}\$" "$input_file" | \
        grep ^SHA256: | sed 's/^SHA256: //')
      if [ "${#sha256sum}" -ne "64" ]; then
        echo "Bad sha256sum from Packages"
        local failed=1
      fi
      echo "$pkg_full" "$sha256sum" >> "$output_file"
    fi
  done
  if [ $failed -eq 1 ]; then
    exit 1
  fi
  # sort -o does an in-place sort of this file
  sort "$output_file" -o "$output_file"
}

######################################################################
# Environment Functions
######################################################################
set_environment_variables() {
  banner setenv
  case ${SELECTED_ARCH} in
    *amd64)
      ARCH=AMD64
      ;;
    *arm64)
      ARCH=ARM64
      ;;
    *)
      echo "ERROR: Unable to determine architecture based on: ${SELECTED_ARCH}"
      exit 1
      ;;
  esac
  ARCH_LOWER=$(echo $ARCH | tr '[:upper:]' '[:lower:]')
}

environment_checks() {
  banner "Environments Checks"
  BUILD_DIR="${SCRIPT_DIR}/out/sysroot-build/${DIST}"
  mkdir -p "${BUILD_DIR}"
  echo "Using build directory: ${BUILD_DIR}"
  for tool in ${REQUIRED_TOOLS} ; do
    if ! command -v "${tool}" > /dev/null ; then
      echo "Required binary $tool not found."
      echo "Exiting."
      exit 1
    fi
  done

  # This is where the staging sysroot is.
  INSTALL_ROOT="${BUILD_DIR}/${DIST}_${ARCH_LOWER}_staging"
  echo "OF: $OUTPUT_FILE"
  TARBALL=${OUTPUT_FILE:-"${BUILD_DIR}/${DISTRO}_${DIST}_${ARCH_LOWER}_sysroot.tar.gz"}

  if ! mkdir -p "${INSTALL_ROOT}" ; then
    echo "ERROR: ${INSTALL_ROOT} can't be created."
    exit 1
  fi
  echo "root"
}


run_main() {
  set_environment_variables
  environment_checks

  echo "ARCH=${ARCH}"
  case ${ARCH} in
    AMD64)
      build_sysroot_amd64
      ;;
    ARM64)
      build_sysroot_arm64
      ;;
    *)
      echo "ERROR: Unable to determine architecture based on: ${SELECTED_ARCH}"
      exit 1
      ;;
  esac
}

######################################################################
# Build Funcs
######################################################################
build_sysroot_amd64() {
  clean_install_dir
  local package_file="${DEBIAN_DEP_LIST_AMD64}"
  generate_package_list_amd64 "$package_file"
  local files_and_sha256sums
  files_and_sha256sums=$(cat "${package_file}")
  strip_checksums_from_package_list "$package_file"
  # shellcheck disable=SC2086
  install_into_sysroot ${files_and_sha256sums}
  cleanup_jail_symlinks
  verify_library_deps_amd64
  create_tarball
}

build_sysroot_arm64() {
  clean_install_dir
  local package_file="${DEBIAN_DEP_LIST_ARM64}"
  generate_package_list_arm64 "$package_file"
  local files_and_sha256sums
  files_and_sha256sums="$(cat "${package_file}")"
  strip_checksums_from_package_list "$package_file"
  # shellcheck disable=SC2086
  install_into_sysroot ${files_and_sha256sums}
#  HacksAndPatchesARM64
  cleanup_jail_symlinks
  verify_library_deps_arm64
  create_tarball
}

generate_package_list_common() {
  local output_file="$1"
  local arch="$2"
  local packages="$3"
  local list_base="${BUILD_DIR}/Packages.${DIST}_${arch}"
  : > "${list_base}"  # Create (or truncate) a zero-length file.
  printf '%s\n' "${APT_SOURCES_LIST[@]}" | while read -r source; do
    generate_package_list_dist "${arch}" "${source}"
  done
  generate_package_list "${list_base}" "${output_file}" "${packages}"
}
generate_package_list_amd64() {
  generate_package_list_common "$1" amd64 "${DEBIAN_PACKAGES}
    ${DEBIAN_PACKAGES_X86:=} ${DEBIAN_PACKAGES_AMD64:=}"
}

generate_package_list_arm64() {
  generate_package_list_common "$1" arm64 "${DEBIAN_PACKAGES}
    ${DEBIAN_PACKAGES_ARM64:=}"
}

install_into_sysroot() {
  banner "Install Libs And Headers Into Jail"

  mkdir -p "${BUILD_DIR}/debian-packages"
  # The /debian directory is an implementation detail that's used to cd into
  # when running dpkg-shlibdeps.
  mkdir -p "${INSTALL_ROOT}/debian"
  # An empty control file is necessary to run dpkg-shlibdeps.
  touch "${INSTALL_ROOT}/debian/control"
  while (( "$#" )); do
    local file="$1"
    local package="${BUILD_DIR}/debian-packages/${file##*/}"
    shift
    local sha256sum="$1"
    shift
    if [ "${#sha256sum}" -ne "64" ]; then
      echo "Bad sha256sum from package list"
      exit 1
    fi

    sub_banner "Installing $(basename "${file}")"
    download_or_copy "${file}" "${package}"
    if [ ! -s "${package}" ] ; then
      echo
      echo "ERROR: bad package ${package}"
      exit 1
    fi
    echo "${sha256sum}  ${package}" | sha256sum --quiet -c

    sub_sub_banner "Extracting to ${INSTALL_ROOT}"
    dpkg-deb -x "${package}" "${INSTALL_ROOT}"

    base_package=$(dpkg-deb --field "${package}" Package)
    mkdir -p "${INSTALL_ROOT}/debian/${base_package}/DEBIAN"
    dpkg-deb -e "${package}" "${INSTALL_ROOT}/debian/${base_package}/DEBIAN"
  done

  # Prune /usr/share, leaving only pkgconfig, wayland, and wayland-protocols.
  # shellcheck disable=SC2010
  ls -d "${INSTALL_ROOT}"/usr/share/* | \
    grep -v "/\(pkgconfig\|wayland\|wayland-protocols\)$" | xargs rm -r
}


cleanup_jail_symlinks() {
  banner "Jail symlink cleanup"

  local savepwd
  savepwd=$(pwd)
  cd "${INSTALL_ROOT}"
  local libdirs=(lib usr/lib lib64)

  # shellcheck disable=SC2086
  find "${libdirs[@]}" -type l -printf '%p %l\n' | while read -r link target; do
    # skip links with non-absolute paths
    echo "${target}" | grep -qs ^/ || continue
    echo "${link}: ${target}"
    # Relativize the symlink.
    prefix=$(echo "${link}" | sed -e 's/[^/]//g' | sed -e 's|/|../|g')
    ln -snfv "${prefix}${target}" "${link}"
  done

  failed=0
  while read -r link target; do
    # Make sure we catch new bad links.
    if [ ! -r "${link}" ]; then
      echo "ERROR: FOUND BAD LINK ${link}"
      ls -l "${link}"
      failed=1
    fi
  done < <(find "${libdirs[@]}" -type l -printf '%p %l\n')

  if [ $failed -eq 1 ]; then
      exit 1
  fi
  cd "$savepwd"
}

verify_library_deps_common() {
  local arch=$1
  local os=$2
  local find_dirs=(
    "${INSTALL_ROOT}/lib/"
    "${INSTALL_ROOT}/lib/${arch}-${os}/"
    "${INSTALL_ROOT}/usr/lib/${arch}-${os}/"
  )

  local needed_libs
  needed_libs="$(
    find "${find_dirs[@]}" -name "*\.so*" -type f -exec file {} \; | \
      grep ': ELF' | sed 's/^\(.*\): .*$/\1/' | xargs readelf -d | \
      grep NEEDED | sort | uniq | sed 's/^.*Shared library: \[\(.*\)\]$/\1/g')"
  local all_libs
  # shellcheck disable=SC2048,2086
  all_libs="$(find ${find_dirs[*]} -printf '%f\n')"
  # Ignore missing libdbus-1.so.0
  all_libs+="$(echo -e '\nlibdbus-1.so.0')"
  local missing_libs
  missing_libs="$(grep -vFxf <(echo "${all_libs}") <(echo "${needed_libs}") || true)"
  if [ -n "${missing_libs}" ]; then
    echo "Missing libraries:"
    echo "${missing_libs}"
    exit 1
  fi
}

verify_library_deps_amd64() {
  verify_library_deps_common x86_64 linux-gnu
}

verify_library_deps_arm64() {
  verify_library_deps_common aarch64 linux-gnu
}

######################################################################
# Helpers
######################################################################
banner() {
  echo "######################################################################"
  echo "$*"
  echo "######################################################################"
}

sub_banner() {
  echo "  * $*"
}

sub_sub_banner() {
  echo "    > $*"
}

create_tarball() {
  banner "Creating tarball ${TARBALL}"
  tar -czf "${TARBALL}" -C "${INSTALL_ROOT}" .
}

clean_install_dir() {
  banner "Clearing dirs in ${INSTALL_ROOT}"
  rm -rf "${INSTALL_ROOT:?}"/*
}

extract_package_xz() {
  local src_file="$1"
  local dst_file="$2"
  local repo="$3"
  xzcat "${src_file}" | grep -E '^(Package:|Filename:|SHA256:) ' |
    sed "s|Filename: |Filename: ${repo}|" > "${dst_file}"
}

generate_package_list_dist_repo() {
  local arch="$1"
  local repo="$2"
  local dist="$3"
  local repo_name="$4"
  local tmp_package_list="${BUILD_DIR}/Packages.${dist}_${repo_name}_${arch}"
  local repo_basedir="${repo}/dists/${dist}"
  local package_list="${BUILD_DIR}/Packages.${dist}_${repo_name}_${arch}.${PACKAGES_EXT}"
  local package_file_arch="${repo_name}/binary-${arch}/Packages.${PACKAGES_EXT}"
  local package_list_arch="${repo_basedir}/${package_file_arch}"
  download_or_copy_unique_filename "${package_list_arch}" "${package_list}"
  verify_package_listing "${package_file_arch}" "${package_list}" "${repo}" "${dist}"
  extract_package_xz "${package_list}" "${tmp_package_list}" "${repo}"
  ./merge_package_lists.py "${list_base}" < "${tmp_package_list}"
}

generate_package_list_dist() {
  local arch="$1"
  # shellcheck disable=SC2086
  set -- $2
  local repo="$1"
  local dist="$2"
  shift 2
  while (( "$#" )); do
      generate_package_list_dist_repo "$arch" "$repo" "$dist" "$1"
      shift
  done
}

strip_checksums_from_package_list() {
  local package_file="$1"
  sed -i 's/ [a-f0-9]\{64\}$//' "$package_file"
}

parse_args() {
  local OPTIND
  # Process the command line arguments.
  while getopts "a:o:h" opt; do
    case ${opt} in
      a)
        SELECTED_ARCH=$OPTARG
        ;;
      o)
        OUTPUT_FILE=$OPTARG
        ;;
      :)
        echo "Invalid option: $OPTARG requires an argument" 1>&2
        ;;
      h)
        usage
        ;;
      *)
        usage
        ;;
    esac
  done
  shift $((OPTIND -1))
}

usage() {
  banner Usage
  echo
  echo 'This script should not be run directly but sourced by the other'
  echo 'scripts (e.g. sysroot_creator_bullseye.sh).  Its up to the parent scripts'
  echo 'to define certain environment variables: e.g.'
  echo ' DISTRO=debian'
  echo ' DIST=bullseye'
  echo ' # Similar in syntax to /etc/apt/sources.list'
  echo ' APT_SOURCES_LIST=( "http://ftp.us.debian.org/debian/ bullseye main" )'
  echo ' KEYRING_FILE=debian-archive-bullseye-stable.gpg'
  echo ' DEBIAN_PACKAGES="gcc libz libssl"'
}


if [ $# -eq 0 ] ; then
  echo "ERROR: you must specify a mode on the commandline"
  echo
  usage
  exit 1
else
  # Change directory to where this script is.
  cd "${SCRIPT_DIR}"
  parse_args "$@"
  run_main
fi
