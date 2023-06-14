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

# Script Usage:
# lint.sh runs all the linters for pixie. Needs Docker.
script_dir="$(dirname "$0")"

# Check if docker exists and works.
if ! docker version &> /dev/null
then
  retval="$?"
  echo "Docker is not working. This might be because you don't have enough permissions, or the docker is not installed."
  exit $retval
fi

"${script_dir}/run_docker.sh" arc lint
