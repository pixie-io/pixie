# Demo applications

These demo applications are packaged to allow `px deploy` to access them. The `manifest.json`
file describes the demo scenarios.

# Instructions for adding a new demo

1. Add a folder containing the demo yaml and license file.
2. Add the demo to the `manifest.json` file.
3. Test the CLI:

```
# 1. (Optional) Update the GCS bucket in the `demos/BUILD.bazel` demo_upload step.

# 2. Upload the demo artifacts to the dev bucket:
bazel run //demos:upload_dev_demo

# 3. Test the CLI with the dev artifacts:
bazel run //src/pixie_cli:px demo list -- -artifacts <DEV_ARTIFACTS_URL>
bazel run //src/pixie_cli:px demo deploy <DEMO_NAME> -- -artifacts <DEV_ARTIFACTS_URL>
```

## Updating the `kafka` demo

1. Clone `https://github.com/pixie-labs/microservice-kafka` and switch to the `pixie` branch.

2. (optional) Build the container images & update the individual yaml files.

3. Build a single yaml file for the demo:

```
kustomize build . >  kafka.yaml
```

3. Copy the yaml file to `pixie/demos/kafka`.
