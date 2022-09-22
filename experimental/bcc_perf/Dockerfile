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

FROM gcr.io/pl-dev-infra/demos/tools/bcc:v1.0


ENV GCSFUSE_REPO=gcsfuse-xenial
RUN echo "deb http://packages.cloud.google.com/apt $GCSFUSE_REPO main" | sudo tee /etc/apt/sources.list.d/gcsfuse.list
RUN apt-get update && apt-get install --no-install-recommends -y \
        curl
RUN curl https://packages.cloud.google.com/apt/doc/apt-key.gpg | sudo apt-key add -
RUN apt-get install --no-install-recommends -y --allow-unauthenticated gcsfuse

WORKDIR /usr/share/bcc/tools
COPY bcc_commands.sh /usr/share/bcc/tools/bcc_commands.sh
RUN chmod +x ./bcc_commands.sh
