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

FROM --platform=$BUILDPLATFORM ubuntu:22.04@sha256:67211c14fa74f070d27cc59d69a7fa9aeff8e28ea118ef3babc295a0428a6d21

ARG BUILDPLATFORM
ARG TARGETARCH
ARG TARGETPLATFORM

RUN echo "I am running on $BUILDPLATFORM, building for $TARGETPLATFORM"

RUN apt-get update
RUN apt-get install -y -q binutils g++-12 gcc-12
RUN apt-get install -y -q binutils-aarch64-linux-gnu g++-12-aarch64-linux-gnu gcc-12-aarch64-linux-gnu

WORKDIR /src
COPY build.sh .
COPY hello.cc .
RUN ./build.sh

RUN apt-get purge --autoremove -y -q binutils g++-12 gcc-12 binutils-aarch64-linux-gnu g++-12-aarch64-linux-gnu gcc-12-aarch64-linux-gnu

CMD /src/hello
