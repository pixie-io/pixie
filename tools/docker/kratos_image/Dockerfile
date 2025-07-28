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

FROM --platform=$BUILDPLATFORM golang:1.24-alpine@sha256:9fadeb603e14f1f3e08bdbec6681fa14446053c498a554f3e57260bf892c487e AS build

ARG TARGETOS TARGETARCH
ARG KRATOS_VERSION

ARG GOOS=$TARGETOS
ARG GOARCH=$TARGETARCH

WORKDIR /src

RUN apk update
RUN apk add git

RUN git clone --depth 1 https://github.com/ory/kratos.git
WORKDIR /src/kratos
RUN git fetch --tags && git checkout $KRATOS_VERSION

ENV GO111MODULE=on
# kratos and hydra require CGO if sqlite is used, but we exclusively use postgres
ENV CGO_ENABLED=0

RUN go mod download

# TODO(ddelnano): Remove once kratos upstream has updated dependencies
RUN go get github.com/golang-jwt/jwt/v4@v4.5.2
RUN go get github.com/golang-jwt/jwt/v5@v5.2.2
RUN go get golang.org/x/crypto@v0.35.0
RUN go get golang.org/x/oauth2@v0.27.0

ARG BUILD_DATE

RUN go build \
  -ldflags="-X 'github.com/ory/kratos/driver/config.Version=${KRATOS_VERSION}' -X 'github.com/ory/kratos/driver/config.Date=${BUILD_DATE}' -X 'github.com/ory/kratos/driver/config.Commit=$(git rev-parse HEAD)'" \
  -o /usr/bin/kratos

FROM gcr.io/distroless/static-debian12:nonroot@sha256:627d6c5a23ad24e6bdff827f16c7b60e0289029b0c79e9f7ccd54ae3279fb45f

COPY --from=build /usr/bin/kratos /usr/bin/kratos
EXPOSE 4433 4434

ENTRYPOINT ["kratos"]
CMD ["serve"]
