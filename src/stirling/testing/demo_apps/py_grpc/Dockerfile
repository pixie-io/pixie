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

FROM python:3.7-slim@sha256:fbb1606214af72cab47a4caf66ac0b94a343613b2a833c022a6de1acfbe84499
ADD requirements.txt requirements.txt
RUN pip install -r requirements.txt
RUN apt-get update \
  && apt-get install -y wget \
  && rm -rf /var/lib/apt/lists/*
RUN wget -O grpc-v1_19_0.tar.gz https://github.com/grpc/grpc/archive/refs/tags/v1.19.0.tar.gz
RUN apt-get remove -y wget
RUN tar -xvf grpc-v1_19_0.tar.gz grpc-1.19.0/examples/python/helloworld
RUN mv grpc-1.19.0/examples/python/helloworld helloworld
RUN rm grpc-v1_19_0.tar.gz
