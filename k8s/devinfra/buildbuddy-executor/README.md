Bazel RBE with Buildbuddy
=========================

To install buildbuddy executor to a k8s cluster, run the following from the workspace root.

```bash
helm repo add buildbuddy https://helm.buildbuddy.io

BB_EXECUTOR_API_KEY=<api_key> \
IMAGE_TAG="$(grep DOCKER_IMAGE_TAG "docker.properties" | cut -d= -f2)" \
envsubst < k8s/devinfra/buildbuddy-executor/values.yaml | \
helm upgrade --install -f - buildbuddy buildbuddy/buildbuddy-executor --create-namespace -n pixie-executors
```
