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

# Update the schema assets.
go-bindata -modtime=1 -ignore=\.go -ignore=\.sh -pkg=complete -o=complete/bindata.gen.go .
go-bindata -modtime=1 -ignore=\.go -ignore=\.sh -ignore=^auth_schema.graphql -pkg=noauth -o=noauth/bindata.gen.go .

tot=$(bazel info workspace)
pushd "${tot}/src/ui"
graphql_ts_gen=$(yarn bin graphql-schema-typescript)
node "${graphql_ts_gen}" generate-ts "${tot}/src/cloud/api/controller/schema" --output src/types/schema.d.ts
popd
