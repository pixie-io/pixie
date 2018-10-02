#!/bin/bash -ex

# Generate CA key:
openssl genrsa -des3 -out ca.key 4096

# Generate CA certificate:
openssl req -new -x509 -days 365 -key ca.key -out ca.crt

# Generate server key:
openssl genrsa -des3 -out server.key 4096

# Generate server signing request:
openssl req -new -key server.key -out server.csr

# Self-sign server certificate:
openssl x509 -req -days 365 -in server.csr -CA ca.crt -CAkey ca.key -set_serial 01 -out server.crt

# Remove passphrase from the server key:
openssl rsa -in server.key -out server.key

# Generate client key:
openssl genrsa -des3 -out client.key 4096

# Generate client signing request:
openssl req -new -key client.key -out client.csr

# Self-sign client certificate:
openssl x509 -req -days 365 -in client.csr -CA ca.crt -CAkey ca.key -set_serial 01 -out client.crt

# Remove passphrase from the client key:
openssl rsa -in client.key -out client.key
