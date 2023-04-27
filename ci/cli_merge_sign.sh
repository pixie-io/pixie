#!/usr/bin/env bash

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

set -ex

artifacts_dir="${ARTIFACTS_DIR:-?}"

printenv

export KEYCHAIN_PATH=pixie.keychain

security create-keychain -p "${KEYCHAIN_PASSWORD}" "${KEYCHAIN_PATH}"
# Remove timeout
security set-keychain-settings "${KEYCHAIN_PATH}"
security unlock-keychain -p "${KEYCHAIN_PASSWORD}" "${KEYCHAIN_PATH}"
security import "${CERT_PATH}" -P "${CERT_PASSWORD}" -A -t cert -f pkcs12 -k "${KEYCHAIN_PATH}"
security set-key-partition-list -S apple-tool:,apple:,codesign: -s -k "${KEYCHAIN_PASSWORD}" "${KEYCHAIN_PATH}"
security default-keychain -s "${KEYCHAIN_PATH}"
security find-identity -v

release_tag=${TAG_NAME##*/v}
bucket="pixie-dev-public"
ARTIFACT_BASE_PATH="https://storage.googleapis.com/${bucket}/cli"

for arch in amd64 arm64
do
  url="${ARTIFACT_BASE_PATH}/${release_tag}/cli_darwin_${arch}_unsigned"
  rm -f "cli_darwin_${arch}_unsigned"
  wget "${url}"
  mv "cli_darwin_${arch}_unsigned" "cli_darwin_${arch}"
done

# Create a universal binary.
lipo -create -output cli_darwin_universal cli_darwin_arm64 cli_darwin_amd64

gon ci/gon.hcl

cp cli_darwin_universal "${artifacts_dir}"
cp cli_darwin_amd64 "${artifacts_dir}"
cp cli_darwin_arm64 "${artifacts_dir}"
