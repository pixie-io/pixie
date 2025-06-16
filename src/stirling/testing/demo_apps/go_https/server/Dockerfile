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

ARG GO_VERSION
FROM alpine:3.20@sha256:de4fe7064d8f98419ea6b49190df1abbf43450c1702eeb864fe9ced453c1cc5f AS certs

RUN apk add --no-cache openssl

WORKDIR /tmp/certs

# Generate private key
RUN openssl ecparam -genkey -name secp384r1 -out server.key && \
    openssl req -new -x509 -sha256 \
        -key server.key \
        -subj "/C=US/ST=California/L=San Francisco/O=Pixie Labs Inc./CN=127.0.0.1:50101" \
        -out server.crt \
        -days 365

# Stage 2: Build Go app and include certs
FROM golang:${GO_VERSION}-alpine as build

ARG GOLANG_X_NET

WORKDIR /app

# Copy source and build
COPY https_server.go .
RUN go mod init https_server && \
    go get golang.org/x/net@${GOLANG_X_NET} && \
    go mod tidy
RUN CGO_ENABLED=0 go build -o https_server .

FROM scratch
COPY --from=build /app /app
COPY --from=certs /tmp/certs/server.crt /etc/ssl/server.crt
COPY --from=certs /tmp/certs/server.key /etc/ssl/server.key

ENTRYPOINT ["/app/https_server"]
CMD ["--cert", "/etc/ssl/server.crt", "--key", "/etc/ssl/server.key"]
