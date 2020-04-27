## TLS Key Generation Instructions

#### Generate private key

```
openssl genrsa -out cpp-tls-server.pem 2048
```

#### Generate self-signed x.509 certificate (PEM-encodings .pem|.crt)

```
openssl req -new -x509 -sha256 -key cpp-tls-server.pem -out cpp-tls-server-cert.pem -days 3650
```

## How to run server and client
There should be no need to regenerate the keys.
```
bazel build //demos/client_server_apps/cpp_tls_client:client
bazel build //demos/client_server_apps/cpp_tls_server:server
cd $PIXIE_ROOT/demos/client_server_apps/cpp_tls_server
./server
```

In a separate tab,
```
cd $PIXIE_ROOT/demos/client_server_apps/cpp_tls_client
./client
```