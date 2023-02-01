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

git clone https://github.com/nats-io/nats-server.git
cd nats-server || exit
# https://github.com/nats-io/nats-server/blob/main/.goreleaser-nightly.yml#L11
CGO_ENABLED=0 go build -o nats-server
gcr_tag="gcr.io/pixie-oss/pixie-dev-public/nats/nats-server"
docker build . -f docker/Dockerfile.nightly -t ${gcr_tag}
docker push ${gcr_tag}
