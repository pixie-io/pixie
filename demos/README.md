# Demo applications

These demo applications are packaged to allow `px deploy` to access them. The `manifest.json`
file describes the demo scenarios.

# General Notes

- For best results when running the continuous profiler, all Java applications should be run with `-XX:+PreserveFramePointer`.

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

# 4. After your PR is merged, upload the demo artifacts to the prod bucket:
bazel run //demos:upload_prod_demo
```

## Updating the `px-kafka` demo

1. Clone `https://github.com/pixie-labs/microservice-kafka` and switch to the `pixie` branch.

2. (optional) Build the container images & update the individual yaml files.

3. Build a single yaml file for the demo:

```
kustomize build . >  kafka.yaml
```

4. Copy the yaml file to `pixie/demos/kafka`.

## Updating the `px-online-boutique` demo

Our custom `adservice` image includes the `-XX:+PreserveFramePointer` Java option. To build our custom `adservice` image:

1. Clone `https://github.com/pixie-labs/microservices-demo` and switch to the `pixie` branch.

2. Run the following commands:

```
cd src/adservice
docker build -t gcr.io/pixie-prod/demos/microservices-demo-app/adservice:1.0 .
docker push gcr.io/pixie-prod/demos/microservices-demo-app/adservice:1.0
```
