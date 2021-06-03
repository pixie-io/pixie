# Development Environment

The Pixie project currently consists of three main components:

- Pixie Cloud, the control plane of Pixie responsible for managing Viziers and hosting the UI.
- Vizier, the data plane of Pixie containing the agents deployed to a K8s cluster.
- CLI, the command line interface which can be used to deploy and query Viziers.

This document outlines the process for setting up the development environment for each of these components.

## Setting up the Environment

To setup the developer environment required to start building Pixie's components, it is easiest to use our docker image which has the necessary tools and packages. If you plan to use a non-minikube environment (gke, eks, etc) for running Vizier/Pixie Cloud, this can be done by running our script:

```
./scripts/run_docker.sh
```

Otherwise, to launch the dev environment with minikube, run:

```
make dev-env-start
```


## Pixie Cloud

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

Deploying a development version of Vizier is a 2-step process. A release-version of Vizier must first be deployed (through the CLI or YAMLs), which will set up specific cluster-secrets that are not deployed via Skaffold. Skaffold can then be run to build and deploy a version of Vizier based on local changes.

1. If deploying Vizier pointing to a dev cloud instance, make sure to export the following environment variables:

    ```
    export PL_CLOUD_ADDR=dev.withpixie.dev:443
    export PL_TESTING_ENV=dev
    ```

2. Deploy Vizier through CLI/YAML. If deploying Vizier to a dev cloud instance, make sure to export the necessary dev-specific environment variables mentioned in `Pixie Cloud` above.

    ```
    px deploy
    ```

3. Wait for the Vizier to successfully connect to the cloud instance, to ensure all secrets and configs have been set up properly.

4. Deploy a local version of Vizier using skaffold.

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

