# Development Environment

The Pixie project currently consists of three main components:

- Pixie Cloud, the control plane of Pixie responsible for managing Viziers and hosting the UI.
- Vizier, the data plane of Pixie containing the agents deployed to a K8s cluster.
- CLI, the command line interface which can be used to deploy and query Viziers.

This document outlines the process for setting up the development environment for each of these components.

## Setting up the Environment

To set up the developer environment required to start building Pixie's components, run the `run_docker.sh` script. The following script will run the Docker container and dump you out inside the docker container console from which you can run all the necessary tools to build, test, and deploy Pixie in development mode.

1. Since this script runs a Docker container, you must have Docker installed. To install it follow these instructions [here](https://docs.docker.com/get-docker/).

1. `run_docker.sh` requires the realpath command which is part of the coreutils package that you may need to install:
    - Ubuntu: `sudo apt-get install coreutils`
    - OS X: `brew install coreutils`

1. Finally, run the following script to start the Docker container:

    ```bash
    ./scripts/run_docker.sh
    ```

## Pixie Cloud

Pixie Cloud manages users, authentication, and proxying “passthrough” mode. If you want to make changes to Pixie Cloud, then you will need to spin up a self-hosted version in development mode to test those changes. If you aren't changing Pixie Cloud, feel free to use officially released Pixie Cloud options listed in our Install Guides.

1. Create the `plc` namespace.

   ```bash
    kubectl create namespace plc
   ```

1. Load the config maps and secrets.

    ```bash
    ./scripts/create_cloud_secrets.sh
    ```

1. Deploy the dependencies.

    ```bash
    kustomize build k8s/cloud_deps/base/elastic/operator | kubectl apply -f -
    kustomize build k8s/cloud_deps/public | kubectl apply -f -
    ```

1. Modify the kustomize file so that `skaffold` can replace the correct image tags.

    ```bash
    perl -pi -e "s|newTag: latest|newTag: \"\"|g" k8s/cloud/public/kustomization.yaml
    perl -pi -e "s|pixie-prod|pixie-dev|g" k8s/cloud/public/kustomization.yaml
    ```

1. Point skaffold to your own image resistry.

    ```bash
    skaffold config set default-repo <your registry>
    ```

1. Deploy the Pixie Cloud services and deployments. This will build the cloud images using changes in the current branch.

    ```bash
    skaffold run -f skaffold/skaffold_cloud.yaml (-p public)
    ```

1. Update `/etc/hosts` so that it knows to point `dev.withpixie.dev` to your running dev cloud instance. The `dev_dns_updater` will do this process for you.

    ```bash
    bazel run //src/utils/dev_dns_updater:dev_dns_updater -- --domain-name "dev.withpixie.dev"
    ```

If connecting a Vizier to a dev version of Pixie cloud, you will need to export the following environment variables:

```bash
export PL_CLOUD_ADDR=dev.withpixie.dev:443
export PL_TESTING_ENV=dev
```

After which, you can rerun a `px auth login` to authenticate with the dev cloud instance, and deploy a new Vizier that points to the dev cloud instance with `px deploy`.
Make sure to `px delete --clobber` if running a prior instance of Vizier pointing to another cloud instance.

## UI

For UI development, refer to the [README](https://github.com/pixie-io/pixie/blob/main/src/ui/README.md).

## Vizier

Vizier is Pixie’s data collector that runs on each cluster. It is responsible for query execution and managing PEMs.

### Getting Started

Verify that you can build Pixie Vizier and run the unit tests.

The following will build Pixie Vizier and run a unit test of your choice in Pixie's Vizier module using [Bazel](https://bazel.build/). If you are running the development environment via Docker, the following should be run inside the Docker container.

```bash
bazel test //src/<path to unit test file> --test_output=errors -j $(nproc)
```

### Deploying

Deploying a development version of Vizier is a 2-step process. An official release-version of Vizier must first be deployed (through the Pixie CLI or YAMLs) and can then be run to build and deploy a local development version of Vizier.

1. If you wish to test development changes made to both Pixie Cloud and Vizier, export the following environment variables that will point to the development Pixie Cloud instance:

    ```bash
    export PL_CLOUD_ADDR=dev.withpixie.dev:443
    export PL_TESTING_ENV=dev
    ```

1. Deploy a local development version of Pixie Vizier using  [Skaffold](https://skaffold.dev/). Note each time you make a code change you will need to run this command to build and deploy the new version.

   ```bash
   skaffold run -f skaffold/skaffold_vizier.yaml --default-repo=<your own image registry>
   ```

   You'll need to patch `imagePullSecrets` of the service accounts of the `pl` namespace to use the
   keys provided by your own image registry service. Usually it involves 2 steps: first create the
   secret, then patch `imagePullSecrets` to use the secret created in the previous step.
   See [instructions](https://kubernetes.io/docs/tasks/configure-pod-container/configure-service-account/#add-imagepullsecrets-to-a-service-account)
   for more details.

## CLI

A development version of the CLI can be run using `bazel`.

As before, if pointing the CLI to a dev cloud instance, please make sure to run the following environment variables:

```bash
export PL_CLOUD_ADDR=dev.withpixie.dev:443
export PL_TESTING_ENV=dev
```

You will be able to run any of the CLI commands using `bazel run`.

- `bazel run //src/pixie_cli:px -- deploy` will be equivalent to `px deploy`
- `bazel run //src/pixie_cli:px -- run px/cluster` is the same as `px run px/cluster`
