# Demo applications

These demo applications are packaged to allow `px deploy` to access them. The `manifest.json`
file describes the demo scenarios.

## General Notes

- For best results when running the continuous profiler, all Java applications should be run with `-XX:+PreserveFramePointer`.

## Instructions for adding a new demo

1. Add a folder containing the demo yaml and license file.
2. Add the demo to the `manifest.json` file.
3. Test the CLI:
    1. (Optional) Update the GCS bucket in the `demos/BUILD.bazel` demo_upload step. Set the artifacts URL appropriately.

        ```shell
        export DEV_DEMO_ARTIFACTS_URL=https://storage.googleapis.com/pl-infra-dev-artifacts/dev-demo-apps
        ```

    2. Upload the demo artifacts to the dev bucket:

        ```shell
        bazel run //demos:upload_dev_demo
        ```

    3. Test the CLI with the dev artifacts:

        ```shell
        px demo list --artifacts "${DEV_DEMO_ARTIFACTS_URL}"
        px demo deploy <DEMO_NAME> --artifacts "${DEV_DEMO_ARTIFACTS_URL}"
        ```

4. After your PR is merged, upload the demo artifacts to the prod bucket:

    ```shell
    bazel run //demos:upload_prod_demo
    ```

## Updating the `px-kafka` demo

1. Clone `https://github.com/pixie-io/microservice-kafka` and switch to the `pixie` branch.

2. (optional) Build the container image & update the individual yaml files.

3. Build a single yaml file for the demo:

    ```shell
    kustomize build . >  kafka.yaml
    ```

4. Copy the yaml file to `pixie/demos/kafka`.

## Updating the `px-k8ssandra` demo

1. Clone `https://github.com/pixie-io/spring-petclinic-reactive`

2. Build the k8s manifests with kustomize

    ```shell
    spring-petclinic-reactive-root-dir $ kustomize build . > petclinic.yaml
    ```

3. Clone `https://github.com/k8ssandra/k8ssandra-operator` and checkout the `v1.6.1` tag.


4. Build the manifests needed to install the CRDs and k8ssandra control plane

    ```shell
    # Your kustomize version must be v4.5.7 or newer for these configs

    k8ssandra-operator-root-dir $ cd config/crd
    # Edit config/crd/kustomization.yaml to append the following:
    # commonLabels:
    #   pixie-demo: px-k8ssandra
    kustomize build . > crd.yaml

    k8ssandra-operator-root-dir $ cd config/deployments/control-plane/cluster-scope
    # Edit config/deployments/control-plane/cluster-scope/kustomization.yaml to append the following:
    # commonLabels:
    #   pixie-demo: px-k8ssandra
    kustomize build . > k8ssandra-control-plane.yaml
    ```

5. Concatenate the yaml files in the order specified below and copy it into `pixie/demos/k8ssandra`.

    ```shell
    cat crd.yaml k8ssandra-control-plane.yaml petclinic.yaml > demos/k8ssandra/k8ssandra.yaml
    ```

6. Replace all occurrences of the k8ssandra-operator namespace. At the time of this writing, the areas to search and replace include the namespace field for a given resource in addition to the cert-manager annotations (cert-manager.io/inject-ca-from). Note: there may be kustomize variables used for the cert-manager.io/inject-ca-from annotation, so ensure all usages reference the px-k8ssandra namespace.

## Updating the `px-mongo` demo

1. Clone `https://github.com/pixie-io/mern-k8s`

2. (optional) Build the container images & update the individual yaml files.

3. Build a single yaml file for the demo:

    ```shell
    kustomize build . > mongodb.yaml
    ```

4. Copy the yaml file to `pixie/demos/mongodb`.

## Updating the `px-finagle` demo

1. Clone `https://github.com/pixie-io/finagle-helloworld`

2. (optional) Build the container images & update the individual yaml files.

3. Build a single yaml file for the demo:

    ```shell
    kustomize . > finagle.yaml
    ```

4. Copy the yaml file to `pixie/demos/finagle`.

## Updating the `px-online-boutique` demo

Our custom `adservice` image includes the `-XX:+PreserveFramePointer` Java option. To build our custom `adservice` image:

1. Clone `https://github.com/pixie-io/microservices-demo` and switch to the `pixie` branch.

2. Run the following commands:

    ```shell
    cd src/adservice
    docker build -t gcr.io/pixie-prod/demos/microservices-demo-app/adservice:1.0 .
    docker push gcr.io/pixie-prod/demos/microservices-demo-app/adservice:1.0
    ```
