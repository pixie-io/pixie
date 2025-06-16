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

# Generate CA key and cert
RUN openssl ecparam -genkey -name secp384r1 -out ca.key && \
    openssl req -x509 -new -nodes -key ca.key -sha256 -days 3650 \
        -subj "/C=US/ST=California/L=San Francisco/O=Pixie Labs Inc./CN=Pixie CA" \
        -out ca.crt

# Generate server key
RUN openssl ecparam -genkey -name secp384r1 -out server.key

# Generate server CSR
RUN openssl req -new -key server.key \
        -subj "/C=US/ST=California/L=San Francisco/O=Pixie Labs Inc./CN=127.0.0.1" \
        -out server.csr

# Create server cert config with SAN and extensions
RUN echo "subjectAltName=IP:127.0.0.1" > server.ext && \
    echo "basicConstraints=CA:FALSE" >> server.ext && \
    echo "keyUsage = digitalSignature, keyEncipherment" >> server.ext && \
    echo "extendedKeyUsage = serverAuth" >> server.ext

# Sign server CSR with CA
RUN openssl x509 -req -in server.csr -CA ca.crt -CAkey ca.key -CAcreateserial \
        -out server.crt -days 365 -sha256 -extfile server.ext

FROM golang:${GO_VERSION}-alpine as build

ARG GOOGLE_GOLANG_GRPC

WORKDIR /app

# Copy source and build
COPY server.go .
COPY greetpb greetpb
RUN go mod init px.dev/pixie/src/stirling/testing/demo_apps/go_grpc_tls_pl/server && \
    go get google.golang.org/grpc@${GOOGLE_GOLANG_GRPC} && \
    go get github.com/gogo/protobuf/proto && \
    go mod tidy
RUN CGO_ENABLED=0 go build -o server .

FROM scratch
COPY --from=certs /tmp/certs/ca.crt /etc/ssl/ca.crt
COPY --from=certs /tmp/certs/server.crt /etc/ssl/server.crt
COPY --from=certs /tmp/certs/server.key /etc/ssl/server.key
COPY --from=build /app/server /app/server

ENTRYPOINT ["/app/server"]
CMD ["--server_tls_cert", "/etc/ssl/server.crt", "--server_tls_key", "/etc/ssl/server.key", "--tls_ca_cert", "/etc/ssl/ca.crt"]
