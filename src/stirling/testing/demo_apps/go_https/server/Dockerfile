FROM alpine:3.20 AS certs

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
FROM golang:1.23.9

WORKDIR /app

# Copy Go mod and install deps
COPY go.mod go.sum ./
RUN go mod download

# Copy source and build
COPY https_server.go .
RUN go build -o https_server .

# Copy certs from previous stage
COPY --from=certs /tmp/certs/server.crt /etc/ssl/server.crt
COPY --from=certs /tmp/certs/server.key /etc/ssl/server.key

ENTRYPOINT ["/app/https_server"]
CMD ["--cert", "/etc/ssl/server.crt", "--key", "/etc/ssl/server.key"]
