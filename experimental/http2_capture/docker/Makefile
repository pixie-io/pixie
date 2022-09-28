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

build: Dockerfile http2_capture.sh
	docker build -t gcr.io/pl-dev-infra/http2-capture .

push: build
	docker push gcr.io/pl-dev-infra/http2-capture:latest

run: push
	docker run -it --init --rm -v /:/host --net=host --privileged gcr.io/pl-dev-infra/http2-capture
