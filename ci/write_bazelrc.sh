#!/bin/bash -ex

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

cp ci/jenkins.bazelrc jenkins.bazelrc

echo "build --remote_header=x-buildbuddy-api-key=${BUILDBUDDY_API_KEY}" >> jenkins.bazelrc
echo "build --action_env=GH_API_KEY=${GH_API_KEY}" >> jenkins.bazelrc

if [[ $JOB_NAME == 'pixie-main/build-and-test-all' ]]; then
  # Only set ROLE=CI if this is running on main. This controls the whether this
  # run contributes to the test matrix at https://bb.corp.pixielabs.ai/tests/
  echo "build --build_metadata=ROLE=CI" >> jenkins.bazelrc
else
  # Don't upload to remote cache if this is not running main.
  echo "build --remote_upload_local_results=false" >> jenkins.bazelrc
  echo "build --build_metadata=ROLE=DEV" >> jenkins.bazelrc
fi
