# Managed by Helm.
## Install instructions:

```
helm repo add buildbuddy https://helm.buildbuddy.io
helm repo update

# Install secrets.
kubectl create namespace buildbuddy
sops -d $(bazel info workspace)/credentials/dev_infra/buildbuddy/sa_secret.yaml | kubectl -n buildbuddy -f -

# Get the config.
sops -d $(bazel info workspace)/credentials/dev_infra/buildbuddy/config.yaml > config.yaml

# Install.
helm install --namespace buildbuddy buildbuddy buildbuddy/buildbuddy-enterprise -f config.yaml

# Upgrade
helm upgrade --namespace buildbuddy buildbuddy buildbuddy/buildbuddy-enterprise -f config.yaml

# Cleanup
rm -f config.yaml

```
