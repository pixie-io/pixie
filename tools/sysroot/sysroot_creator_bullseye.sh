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
#   https://chromium.googlesource.com/chromium/src/build/+/refs/heads/main/linux/sysroot_scripts/sysroot-creator-bullseye.sh.

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

export DISTRO=debian
export DIST=bookworm

# This number is appended to the sysroot key to cause full rebuilds.  It
# should be incremented when removing packages or patching existing packages.
# It should not be incremented when adding packages.
export SYSROOT_RELEASE=0

ARCHIVE_TIMESTAMP=20221210T034654Z
#20221105T211506Z
ARCHIVE_URL="https://snapshot.debian.org/archive/debian/$ARCHIVE_TIMESTAMP/"
export APT_SOURCES_LIST=(
  # Debian 12 (Bookworm) is needed for GTK4.  It should be kept before bullseye
  # so that bullseye takes precedence.
  "${ARCHIVE_URL} bookworm main contrib non-free"
  "${ARCHIVE_URL} bookworm-updates main contrib non-free"
  "${ARCHIVE_URL} bookworm-backports main contrib non-free"
  # This mimicks a sources.list from bullseye.
#  "${ARCHIVE_URL} bullseye main contrib non-free"
#  "${ARCHIVE_URL} bullseye-updates main contrib non-free"
#  "${ARCHIVE_URL} bullseye-backports main contrib non-free"
)

# gpg keyring file generated using generate_keyring.sh
export KEYRING_FILE="${SCRIPT_DIR}/keyring.gpg"

# Sysroot packages: these are the packages needed to build Pixie.
export DEBIAN_PACKAGES="\
  liblzma-dev
  liblzma5
  libunwind-dev
  libncurses6
  libncurses-dev
  libtinfo6
  libncursesw6
  libatomic1
  libc6
  libc6-dev
  libcrypt-dev
  libcrypt1
  libdrm-amdgpu1
  libdrm-dev
  libdrm-nouveau2
  libdrm-radeon1
  libdrm2
  libelf-dev
  libelf1
  libgcc-12-dev
  libgcc-s1
  libgomp1
  libicu-dev
  libicu72
  libpciaccess0
  libstdc++-12-dev
  libstdc++6
  linux-libc-dev
  zlib1g
  zlib1g-dev
  libunwind8
"

#  libicu67
export DEBIAN_PACKAGES_AMD64="
  libitm1
  liblsan0
  libtsan2
  libasan8
  libquadmath0
  libubsan1
  libdrm-intel1
"

export DEBIAN_PACKAGES_ARM64="
  libitm1
  liblsan0
  libtsan2
  libasan8
  libubsan1
  libdrm-tegra0
  libdrm-freedreno1
  libdrm-etnaviv1
  libhwasan0
"


# shellcheck disable=SC1090,SC1091
. "${SCRIPT_DIR}/sysroot_creator.sh"
