#!/bin/bash -e

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

usage() {
  echo "Usage: $0 <artifact_type> <version> <url>"
}

if [ $# -lt 3 ]; then
  usage
  exit 1
fi
artifact_type="$1"
version="$2"
url="$3"

readme_path="README.md"

latest_release_comment="<!--${artifact_type}-latest-release-->"

pretty_artifact_name() {
  case "${artifact_type}" in
    cli) echo "CLI";;
    vizier) echo "Vizier";;
    operator) echo "Operator";;
    cloud) echo "Cloud";;
  esac
}

latest_release_line() {
  echo "- [$(pretty_artifact_name) ${version}](${url})${latest_release_comment}"
}

sed -i 's|.*'"${latest_release_comment}"'.*|'"$(latest_release_line)"'|' "${readme_path}"

echo "[bot][releases] Update readme with link to latest ${artifact_type} release." > pr_title
cat <<EOF > pr_body
Summary: TSIA

Type of change: /kind cleanup

Test Plan: N/A
EOF
