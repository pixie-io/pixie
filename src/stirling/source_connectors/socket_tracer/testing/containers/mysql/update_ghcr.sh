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

# Increment this number on every upload.
version=1.3
tag="ghcr.io/pixie-io/python_mysql_connector:$version"

docker build . -t $tag
docker push $tag


sha=$(docker inspect --format='{{index .RepoDigests 0}}' $tag | cut -f2 -d'@')

echo ""
echo "Image pushed!"
echo "IMPORTANT: Now update //bazel/container_images.bzl with the following digest: $sha"
