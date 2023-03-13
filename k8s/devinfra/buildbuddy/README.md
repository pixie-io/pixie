# Managed by Helm

## Install instructions

The buildbuddy charts mark nginx ingress and cert-manager as subcharts. However if we are installing buildbuddy in a kubernetes cluster that already has these installed, they don't play nice with each other. Besides buildbuddy installs these deps in the buildbuddy namespace whereas one probably wants to stick ingress and cert-manager in their own unique namespaces. So I highly recommend skipping installing these from the buildbuddy charts and instead loading them on the side.

Note: Ignoring the subcharts by setting `certmanager.enabled`, and `ingress.enabled` to `false` also causes relevant resources to not get created, however we do want most of the resulting resources. So this requires some surgery by checking out the chart repo and manually modifying the `requirements.yaml`.

```shell
helm repo add buildbuddy https://helm.buildbuddy.io
helm repo update
```

### Install CRDs

If installing cert-manager's deps with buildbuddy, make sure to apply the cert manager CRDs.

```shell
kubectl apply --validate=false -f https://github.com/jetstack/cert-manager/releases/download/v1.6.1/cert-manager.crds.yaml
```

### Install secrets

```shell
kubectl create namespace buildbuddy
sops -d $(git rev-parse --show-toplevel)/private/credentials/dev_infra/buildbuddy/sa_secret.yaml | kubectl apply -n buildbuddy -f -
```

### Get the config

```shell
sops -d $(git rev-parse --show-toplevel)/private/credentials/dev_infra/buildbuddy/config.yaml > config.yaml
```

### Install

```shell
helm install --namespace buildbuddy buildbuddy buildbuddy/buildbuddy-enterprise -f config.yaml
```

### Upgrade

```shell
helm upgrade --namespace buildbuddy buildbuddy buildbuddy/buildbuddy-enterprise -f config.yaml
```

### Cleanup

```shell
rm -f config.yaml
```
