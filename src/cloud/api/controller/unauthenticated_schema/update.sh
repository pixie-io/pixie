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
go generate

# The following assumes a few things:
# - yarn appears in $PATH (likely from running `npm i -g yarn` after getting NodeJS set up)
# - The package `graphql-schema-typescript` still has carriage returns in its bin script, breaking it on not-Windows.
# - `yarn install` was already run in //src/ui
# - The environment running this script can run Bash (if the top of this file is any indication, it can).
schemaRoot="$(cd "$(dirname "$0")" && pwd)" # dirname $0 can come back as just `.`, resolve it to a real path.
uiRoot="${schemaRoot}/../../../../ui"
pushd "${uiRoot}" && yarn regenerate_graphql_schema ../cloud/api/controller/unauthenticated_schema/schema.graphql --output ../cloud/api/controller/unauthenticated_schema/schema.d.ts
