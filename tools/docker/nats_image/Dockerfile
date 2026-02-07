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

FROM --platform=$BUILDPLATFORM golang:1.24-alpine@sha256:c8c5f95d64aa79b6547f3b626eb84b16a7ce18a139e3e9ca19a8c078b85ba80d AS build

ARG TARGETOS TARGETARCH
ARG NATS_VERSION

ARG GOOS=$TARGETOS
ARG GOARCH=$TARGETARCH

WORKDIR /src

RUN apk update
RUN apk add git

RUN git clone --depth 1 https://github.com/nats-io/nats-server.git
WORKDIR /src/nats-server
RUN git fetch --tags && git checkout $NATS_VERSION

ARG GO111MODULE=on
ARG CGO_ENABLED=0

RUN go mod download
# TODO(ddelnano): Remove once NATS server is updated to have
# vulnerability free upstream deps
RUN go get golang.org/x/crypto@v0.35.0
RUN go get github.com/nats-io/nkeys@v0.4.6
RUN go build -trimpath -ldflags "-X github.com/nats-io/nats-server/v2/server.gitCommit=$(git rev-parse --short HEAD)" -o ./nats-server

FROM scratch
COPY --from=build /src/nats-server/nats-server /nats-server
COPY nats-server.conf /nats-server.conf
EXPOSE 4222 8222 6222
ENV PATH="$PATH:/"
ENTRYPOINT ["/nats-server"]
CMD ["--config", "nats-server.conf"]
