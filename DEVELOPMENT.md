# Development Environment

The Pixie project currently consists of three main components:

- Pixie Cloud, the control plane of Pixie responsible for managing Viziers and hosting the UI.
- Vizier, the data plane of Pixie containing the agents deployed to a K8s cluster.
- CLI, the command line interface which can be used to deploy and query Viziers.

This document outlines the process for setting up the development environment for each of these components.

## Setting up the Environment
There are two different ways to run a development environment. The first and easiest approach is via Docker. If you plan to use a non-Minikube environment (GKE, EKS, etc) for running Vizier/Pixie Cloud, use this first approach via Docker. The second approach is using Make and Minikube.

### Running Via Docker
To set up the developer environment required to start building Pixie's components, it is easiest to run the `run_docker.sh` script. The following script will run the Docker container and dump you out inside the docker container console from which you can run all the necessary tools to build, test, and deploy Pixie in development mode.

1. Since this script runs a Docker container, you must have Docker installed. To install it follow these instructions [here](https://docs.docker.com/get-docker/).

1. `run_docker.sh` requires the realpath command which is part of the coreutils package that you may need to install:
    * Ubuntu: `sudo apt-get install coreutils`
    * OS X: `brew install coreutils`

1. Finally, run the following script to start the Docker container:
    ```
    ./scripts/run_docker.sh
    ```

1. Since development of Pixie requires a Kubernetes cluster to deploy to, you must have Minikube installed and running. Follow the instructions [here](https://docs.px.dev/installing-pixie/setting-up-k8s/minikube-setup/). Note that Pixie development scripts use the output of `kubectl config current-context` to determine which Kubernetes cluster to deploy to. So make sure if you have multiple clusters, the context is pointing to the correct target cluster.


### Running Via Minikube
If you plan on using a Minikube environment, launch the dev environment with Minikube, via Make.

1. Since this runs via Minikube, you must have Minikube installed. To install it follow the instructions [here](https://docs.px.dev/installing-pixie/setting-up-k8s/minikube-setup/).

1. Run make to spin up a Minikube environment:
    ```
    make dev-env-start
    ```


## Pixie Cloud
Pixie Cloud manages users, authentication, and proxying “passthrough” mode. If you want to make changes to Pixie Cloud, then you will need to spin up a self-hosted version in development mode to test those changes. If you aren't changing Pixie Cloud, feel free to use officially released Pixie Cloud options listed in our Install Guides.

1. Load the config maps and secrets.

    ```
    ./scripts/deploy_cloud_prereqs.sh plc-dev dev
    ```
2. Deploy the Pixie Cloud services and deployments. Note to add profile flags for whether you're running a dev build, minikube env, or want to use ory_auth in place of auth0.

    ```
    # note: Profile args are not exclusive.
    # -p dev enables the dev profile
    # -p minikube enables the minikube profile
    # -p ory_auth enables the ory authentication deployment. Not including this uses auth0 by default.
    skaffold dev -f skaffold/skaffold_cloud.yaml (-p dev) (-p minikube) (-p ory_auth)
    ```
3. Load basic artifacts into the database.

    ```
    ./scripts/load_dev_db.sh plc-dev
    ```
4. Update `/etc/hosts` so that it knows to point `dev.withpixie.dev` to your running dev cloud instance. The `dev_dns_updater` will do this process for you.

    ```
    bazel run //src/utils/dev_dns_updater:dev_dns_updater -- --domain-name "dev.withpixie.dev"
    ```

5. (`ory_auth` only) Create the admin user and get link to update password
    ```
    skaffold dev -f skaffold/skaffold_cloud.yaml -p create_admin_job
    ```
    And click the link from the logs:
    ```
    ...
    [create-admin-job-hssrl create-admin-job] time="2021-04-19T19:35:56Z" level=info msg="Please go to 'https://work.dev.withpixie.dev/oauth/kratos/self-service/recovery/methods/link?flow=31e0cef8-43ad-4a7a-b2e8-1d59a1101527&token=RRwpPGtJXzuNFjffxize1HZppp7oS3e3' to set password for 'admin@default.com'" func=main.main file="src/cloud/jobs/create_admin_user/main.go:100"
    ```
6. (`ory_auth` only) Create Hydra OAuth Client
   ```
    $ export HYDRA_POD=$(kubectl get pods -nplc-dev -l name=hydra --template '{{range .items}}{{.metadata.name}}{{end}}')
    $ export HYDRA_SECRET=<your secret here>
    $ kubectl exec -n plc-dev $HYDRA_POD -it -- hydra clients create \
        --endpoint https://hydra.plc-dev.svc.cluster.local:4445 --id auth-code-client \
        --secret $HYDRA_SECRET \
        --grant-types authorization_code,refresh_token,implicit \
        --response-types code,id_token,token \
        --scope openid,offline,vizier \
        --callbacks https://dev.withpixie.dev/oauth/auth/callback \
        --callbacks https://work.dev.withpixie.dev/auth/callback\?mode\=ui \
        --callbacks https://work.dev.withpixie.dev/auth/callback\?mode\=ui\&signup\=true \
        --callbacks https://work.dev.withpixie.dev/auth/callback\?mode\=cli_get\&redirect_uri\=http%3A%2F%2Flocalhost%3A8085%2Fauth_complete \
        --callbacks https://work.dev.withpixie.dev/auth/callback\?mode\=cli_token \
        --callbacks https://work.dev.withpixie.dev:8080/auth/callback\?mode\=ui \
        --callbacks https://work.dev.withpixie.dev:8080/auth/callback\?mode\=ui\&signup\=true \
        --callbacks https://work.dev.withpixie.dev:8080/auth/callback\?mode\=cli_token \
        --callbacks https://work.dev.withpixie.dev:8080/auth/callback\?mode\=cli_get \
        --callbacks https://work.dev.withpixie.dev:8080/auth/callback\?mode\=cli_get\&redirect_uri\=http%3A%2F%2Flocalhost%3A8085%2Fauth_complete \
        --skip-tls-verify
    ```

If connecting a Vizier to a dev version of Pixie cloud, you will need to export the following environment variables:

```
export PL_CLOUD_ADDR=dev.withpixie.dev:443
export PL_TESTING_ENV=dev
```

After which, you can rerun a `px auth login` to authenticate with the dev cloud instance, and deploy a new Vizier that points to the dev cloud instance with `px deploy`.
Make sure to `px delete --clobber` if running a prior instance of Vizier pointing to another cloud instance.

## Vizier
Vizier is Pixie’s data collector that runs on each cluster. It is responsible for query execution and managing PEMs.

### Getting Started
Verify that you can build Pixie Vizier and run the unit tests.

The following will build Pixie Vizier and run a unit test of your choice in Pixie's Vizier module. If you are running the development environment via Docker, the following should be run inside the Docker container.

```
bazel test //src/<path to unit test file> --test_output=errors -j $(nproc)
```

### Deploying
Deploying a development version of Vizier is a 2-step process. An official release-version of Vizier must first be deployed (through the Pixie CLI or YAMLs) and Skaffold can then be run to build and deploy a local development version of Vizier.

1. If you wish to test development changes made to both Pixie Cloud and Vizier, export the following environment variables that will point to the development Pixie Cloud instance:

    ```
    export PL_CLOUD_ADDR=dev.withpixie.dev:443
    export PL_TESTING_ENV=dev
    ```

1. Install the Pixie CLI and run `px deploy`. Depending on whether you are pointing to a self-hosted Pixie Cloud or the official Community Pixie Cloud follow the appropriate installation guide [here](https://docs.px.dev/installing-pixie/install-guides/). `px deploy` will set up specific cluster-secrets, etc that are not deployed via Skaffold. Wait for this command to successfully complete and Vizier to successfully connect to Pixie Cloud, to ensure all secrets and configs have been set up properly. Note you will not need to run this to deploy again unless you connect to a different cluster.

1. Deploy a local development version of Pixie Vizier using Skaffold. Note each time you make a code change you will need to run this command to build and deploy the new version.

   ```
   skaffold run -f skaffold/skaffold_vizier.yaml
   ```

## CLI

A development version of the CLI can be run using `bazel`.

As before, if pointing the CLI to a dev cloud instance, please make sure to run the following environment variables:

```
export PL_CLOUD_ADDR=dev.withpixie.dev:443
export PL_TESTING_ENV=dev
```

You will be able to run any of the CLI commands using `bazel run`.

- `bazel run //src/pixie_cli:px -- deploy` will be equivalent to `px deploy`
- `bazel run //src/pixie_cli:px -- run px/cluster` is the same as `px run px/cluster`
