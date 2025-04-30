Bazel RBE with Buildbuddy
=========================

To install buildbuddy executor to a k8s cluster, run the following from the workspace root.

```bash
export BB_EXECUTOR_API_KEY=""
export NAMESPACE=""

helm repo add buildbuddy https://helm.buildbuddy.io

kubectl create namespace "${NAMESPACE}" || true
kubectl create secret -n "${NAMESPACE}" generic buildbuddy-executor-api-key --from-literal=api-key="${BB_EXECUTOR_API_KEY}" || true

IMAGE_TAG="$(grep DOCKER_IMAGE_TAG "docker.properties" | cut -d= -f2)" \
envsubst '$IMAGE_TAG' < k8s/devinfra/buildbuddy-executor/values.yaml | \
helm upgrade --install -f - buildbuddy buildbuddy/buildbuddy-executor -n "${NAMESPACE}"
```
