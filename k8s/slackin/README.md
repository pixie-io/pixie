# Slackin

Slackin is our slack signup helper. More info [here](https://github.com/pixie-io/slackin).

## Deployment instructions

The deploy the app run the following from PIXIE_ROOT:

```bash
kubectl create namespace slackin
sops -d private/credentials/k8s/slackin/slackin_secrets.yaml | kubectl apply -f -
sops -d private/credentials/k8s/slackin/slackin_config.yaml | kubectl apply -f -
kustomize build k8s/slackin | kubectl apply -f -
```

## Cert-manager and Ingress-Nginx

Install cert-manager and Ingress-Nginx Controller.
See [link](https://cert-manager.io/docs/tutorials/acme/nginx-ingress/) for an example.

```shell
helm repo add ingress-nginx https://kubernetes.github.io/ingress-nginx
helm repo update
helm upgrade --install ingress-nginx ingress-nginx \
  --repo https://kubernetes.github.io/ingress-nginx \
  --namespace ingress-nginx --create-namespace
```

WARNING: Ensure that cert-manager isn't already installed before installing it.

```shell
helm repo add jetstack https://charts.jetstack.io
kubectl apply -f https://github.com/cert-manager/cert-manager/releases/download/v1.12.0/cert-manager.crds.yaml
helm install \
  cert-manager jetstack/cert-manager \
  --namespace cert-manager \
  --create-namespace \
  --version v1.12.0
```

Add a ClusterIssuer for cert-manager, see [here](https://cert-manager.io/docs/tutorials/acme/nginx-ingress/#step-6---configure-a-lets-encrypt-issuer).

Deploy the ingress without the TLS config

```shell
kubectl apply -f k8s/slackin/ingress_no_tls.yaml
```

Setup DNS for slackin to point to the nginx-ingress external IP.
Ensure that the site is reachable via the domain name.

Switch to the ingress with TLS and wait for Cert-Manager to get certs for the ingress.

```shell
kubectl apply -f k8s/slackin/ingress.yaml
```
