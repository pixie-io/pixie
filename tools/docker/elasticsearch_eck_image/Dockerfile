# Copyright (c) 2025 Pixie Labs, Inc.
# Licensed under the Apache License, Version 2.0 (the "License")

ARG BUILDPLATFORM

# Build stage
FROM --platform=$BUILDPLATFORM golang:1.24.5-alpine AS builder

# Install necessary packages
RUN apk add --no-cache git make ca-certificates

# Set up cross-compilation variables
ARG TARGETOS TARGETARCH
ARG ECK_VERSION
ARG GOOS=$TARGETOS
ARG GOARCH=$TARGETARCH

# Build arguments from upstream
ARG VERSION
ARG SHA1
ARG SNAPSHOT
ARG GO_TAGS
ARG LICENSE_PUBKEY

# Clone the ECK repository
WORKDIR /src
RUN git clone https://github.com/elastic/cloud-on-k8s.git .
RUN git checkout v${ECK_VERSION}

# Download dependencies
RUN go mod download

# TODO: Add your go get commands here to upgrade vulnerable dependencies
# Example:
# RUN go get github.com/some/package@vX.Y.Z

# Build the operator
RUN CGO_ENABLED=0 GOOS=$GOOS GOARCH=$GOARCH \
    go build \
    -mod readonly \
    -tags="$GO_TAGS" \
    -ldflags="-s -w \
        -X github.com/elastic/cloud-on-k8s/v2/pkg/about.version=${VERSION} \
        -X github.com/elastic/cloud-on-k8s/v2/pkg/about.buildHash=${SHA1} \
        -X github.com/elastic/cloud-on-k8s/v2/pkg/about.buildDate=$(date -u +'%Y-%m-%dT%H:%M:%SZ') \
        -X github.com/elastic/cloud-on-k8s/v2/pkg/about.buildSnapshot=${SNAPSHOT}" \
    -o elastic-operator \
    github.com/elastic/cloud-on-k8s/v2/cmd

# Runtime stage
FROM scratch

# Copy CA certificates for HTTPS connections
COPY --from=builder /etc/ssl/certs/ca-certificates.crt /etc/ssl/certs/ca-certificates.crt

# Copy the operator binary
COPY --from=builder /src/elastic-operator /elastic-operator

# Copy any necessary configuration files from the upstream
# Note: You may need to adjust these paths based on your needs
COPY --from=builder /src/config/eck.yaml /conf/eck.yaml

# Add OCI labels
LABEL \
  org.opencontainers.image.source="https://github.com/elastic/cloud-on-k8s" \
  org.opencontainers.image.title="Elastic Cloud on Kubernetes" \
  org.opencontainers.image.vendor="Elastic" \
  org.opencontainers.image.version="${ECK_VERSION}" \
  org.opencontainers.image.description="Elastic Cloud on Kubernetes (ECK) with upgraded dependencies"

# Use numeric UID for nonroot user
USER 65534

ENTRYPOINT ["/elastic-operator"]
CMD ["manager"]