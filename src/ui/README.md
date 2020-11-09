# Steps to run the UI locally and point to the staging cloud backend:

## Export environment variables for webpack
```
export PL_GATEWAY_URL="https://$(dig +short prod.withpixie.ai @8.8.8.8)"
export PL_BUILD_TYPE=prod
export SELFSIGN_CERT_FILE="$HOME/.prod.cert"
export SELFSIGN_CERT_KEY="$HOME/.prod.key"
```


## Generate self signed certs for SSL
You should only need to do this once.
```
mkcert -install
mkcert \
    -cert-file $SELFSIGN_CERT_FILE \
    -key-file $SELFSIGN_CERT_KEY \
    staging.withpixie.dev "*.staging.withpixie.dev" localhost 127.0.0.1 ::1
```

## Add the following domain to /etc/hosts, or /private/etc/hosts for Mac
Replace site-name with your test site name.
```
127.0.0.1 staging.withpixie.dev <site-name>.staging.withpixie.dev id.staging.withpixie.dev
```

## Run the webpack devserver
```
cd src/ui
yarn install
./node_modules/.bin/webpack-dev-server
```