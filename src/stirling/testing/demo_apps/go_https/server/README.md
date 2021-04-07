# TLS Key Generation Instructions

The currently checked in keys are valid for 127.0.0.1:50101, so unless you are using the server on a different address or port, there is no need to regenerate them.

If, however, there is a need to regenerate the keys, follow the process below:

#### Generate private key

```
openssl ecparam -genkey -name secp384r1 -out https-server.key
```

#### Generate self-signed x.509 certificate (PEM-encodings .pem|.crt)

```
openssl req -new -x509 -sha256 -key https-server.key -out https-server.crt -days 3650
```
Answer the questions, with the most important being the hostname. Use the hostname where the server will run (e.g. 127.0.0.1:50101).

The certificate contains the public key corresponding to the private key.
