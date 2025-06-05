# Steps to run the UI locally and point to the prod cloud backend:

## Export environment variables for webpack
```
export PL_GATEWAY_URL="https://$(dig +short work.getcosmic.ai @8.8.8.8)"
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
    work.getcosmic.ai "*.work.getcosmic.ai" localhost 127.0.0.1 ::1
```

## Add the following domain to /etc/hosts, or /private/etc/hosts for Mac
Replace site-name with your test site name.
```
127.0.0.1 work.getcosmic.ai test.work.getcosmic.ai id.work.getcosmic.ai
```

## Run the webpack devserver
```
cd src/ui
yarn install
yarn dev
```
This will expose the UI locally at 8080

## Access the frontend on the browser
Navigate to https://work.getcosmic.ai:8080/
Note the https and port. If you are not logged in, log in at work.getcosmic.ai because
as of writing this, auth0 doesn't accept callbacks to work.getcosmic.ai:8080

## Note if you are tunneling or get HSTS exceptions
(please do this at your own risk)
in Chrome, navigate to
chrome://net-internals/#hsts and delete the HSTS rules for work.getcosmic.ai

This will then unblock the security feature for this domain. Please ensure to remove this once you are done.


## For a remote VM 
### openSSH client
```
ssh -i privkey user@IP -D 8080
```
### gcloud
```
export instancename="instance-pixie-dev"
export project="gcp-project-uuid"
export zone="europe-west1-d"
gcloud compute ssh $instancename --zone $zone --project $project -- -NL 8080:localhost:8080
```
