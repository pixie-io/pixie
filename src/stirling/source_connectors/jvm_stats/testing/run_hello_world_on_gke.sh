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

bazel run //src/stirling/source_connectors/jvm_stats/testing:push_hello_world_image

# kubectl apply does not force re-pulling the image, as the tag has not changed.
# We have to delete and then apply.
kubectl delete -f src/stirling/source_connectors/jvm_stats/testing/hello_world.yaml
kubectl apply -f src/stirling/source_connectors/jvm_stats/testing/hello_world.yaml
