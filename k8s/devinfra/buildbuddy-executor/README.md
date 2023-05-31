Install buildbuddy executor to a k8s cluster:
- `helm repo add buildbuddy https://helm.buildbuddy.io`
- (For now not all changes to buildbuddy-executor have landed in the helm repo, so you should use my fork [here](https://github.com/JamesMBartlett/buildbuddy-helm/tree/all_changes_for_pixie) and replace any references to `buildbuddy/buildbuddy-executor` with `/path/to/checkout-of-fork/charts/buildbuddy-executor`)
- From the workspace root run:
```
BB_EXECUTOR_API_KEY=<api_key> \
IMAGE_TAG="$(grep DOCKER_IMAGE_TAG "docker.properties" | cut -d= -f2)" \
envsubst < k8s/devinfra/buildbuddy-executor/values.yaml | \
helm upgrade --install -f - buildbuddy buildbuddy/buildbuddy-executor --create-namespace -n buildbuddy
```
