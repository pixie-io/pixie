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

if (($# > 1)); then
  echo "Needs at most one argument as a build label, exit ..."
  exit 1
fi

git_root=$(git rev-parse --show-toplevel)
if [[ "${git_root}" != "$(pwd)" ]]; then
  echo "Must run this at the top of tree of the git repo, exit ..."
  exit 1
fi

function label_to_path() {
  path="${1#"//"}"
  echo "${path/://}"
}

function update_one_build_label() {
  build_label="$1"
  echo "Updating ${build_label} ..."

  # Exits with message if the bazel build command goes wrong.
  if ! out=$(bazel build --color=yes "${build_label}" 2>&1); then
    echo "${out}" >&2
    exit 1
  fi

  srcPath=$(dirname "$(label_to_path "${build_label}")")
  # TODO(nick,PC-738): When moving the UI code to its own package (as opposed to the root), update this path.
  uiTypesDir="src/ui/src/types/generated"
  apiTypesDir="src/ui/packages/pixie-api/src/types/generated"

  # TODO(nick): pixie-api may not require all grpc_web protos; we should not copy over unused files.
  abs_paths=$(find "bazel-bin/${srcPath}" -iregex ".*/[^/]*_pb.*[tj]s")
  if [[ "${abs_paths}" == "" ]]; then
    echo "Failed to locate TypeScript and Javascript Proto files in bazel-bin/${srcPath}"
    return 1
  fi

  # VizierapiServiceClient.ts has a relative import; we're copying elsewhere. We fix this with perl string substitution.
  regexRelativeImport="s|import \* as ([^ ]+) from '([^ /]+/)+vizierapi_pb'\;|import * as \1 from './vizierapi_pb';|m"
  # vizierapi_pb.d.ts incorrectly includes an unused (and non-existent) relative import related to Go protos. Remove it.
  regexExtraneousImport="s|^import \* as github_com_gogo_protobuf_gogoproto_gogo_pb.*$||m"

  for abs_path in $abs_paths; do
    echo "Propagating ${abs_path} ..."
    fname=$(basename "$abs_path")
    cp -f "${abs_path}" "${uiTypesDir}"
    cp -f "${abs_path}" "${apiTypesDir}"

    # Using Perl instead of sed because BSD and GNU treat the -i flag differently: https://stackoverflow.com/a/22247781
    perl -pi -e "${regexRelativeImport}" "${uiTypesDir}/${fname}"
    perl -pi -e "${regexRelativeImport}" "${apiTypesDir}/${fname}"
    perl -pi -e "${regexExtraneousImport}" "${uiTypesDir}/${fname}"
    perl -pi -e "${regexExtraneousImport}" "${apiTypesDir}/${fname}"
  done
}

function update_all_build_labels() {
  # Normally, we would loop over `bazel query "kind('grpc_web_library rule', //src/...)"` to find all grpc_web rules.
  # However, vizier_pl_grpc_web_proto exists in both internal and public dirs, which would cause a clash.
  # We only need the public one for TypeScript, and only two specific files.
  # The internal one that we're skipping is "//src/vizier/vizierpb:vizier_pl_grpc_web_proto"
  update_one_build_label "//src/api/public/vizierapipb:vizier_pl_grpc_web_proto"
  update_one_build_label "//src/shared/vispb:vis_pl_grpc_web_proto"
}

if [[ $# == 0 ]]; then
  update_all_build_labels
elif [[ $# == 1 ]]; then
  build_label="$1"
  if [[ ! "${build_label}" == "//"* ]]; then
    echo "Build label must be fully qualified with '//' at the beginning, exit ..." >&2
    exit 1
  fi
  if [[ "${build_label}" != *"pl_grpc_web_proto" ]]; then
    echo "Expect the build label ends with 'pl_grpc_web_proto', exit ..." >&2
    exit 1
  fi
  update_one_build_label "$1"
fi
