# Steps to run the UI locally and point to the prod cloud backend:

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
    prod.withpixie.ai "*.prod.withpixie.ai" localhost 127.0.0.1 ::1
```

## Add the following domain to /etc/hosts, or /private/etc/hosts for Mac
Replace site-name with your test site name.
```
127.0.0.1 prod.withpixie.ai <site-name>.prod.withpixie.ai id.prod.withpixie.ai
```

## Run the webpack devserver
```
cd src/ui
yarn install
yarn dev
```

## Access the frontend on the browser
Navigate to https://prod.withpixie.ai:8080/
Note the https and port. If you are not logged in, log in at work.withpixie.ai because
as of writing this, auth0 doesn't accept callbacks to prod.withpixie.ai:8080
