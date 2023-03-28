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

FROM ubuntu:22.04@sha256:67211c14fa74f070d27cc59d69a7fa9aeff8e28ea118ef3babc295a0428a6d21

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update -y --fix-missing
RUN apt-get install -y curl

WORKDIR /data
RUN curl -L https://github.com/graalvm/graalvm-ce-builds/releases/download/vm-22.3.0/graalvm-ce-java17-linux-amd64-22.3.0.tar.gz --output dist.tar.gz

WORKDIR /opt
RUN tar -xzf /data/dist.tar.gz
RUN /opt/graalvm-ce-java17-22.3.0/bin/gu install native-image

WORKDIR /opt
VOLUME /archive
CMD ["sh", "-c", "tar -C graalvm-ce-java17-22.3.0 -czf /archive/graalvm-native-image-22.3.0.tar.gz ."]
