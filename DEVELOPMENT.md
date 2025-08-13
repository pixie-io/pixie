# Development Environment

The Pixie project currently consists of three main components:

- Pixie Cloud, the control plane of Pixie responsible for managing Viziers and hosting the UI.
- Vizier, the data plane of Pixie containing the agents deployed to a K8s cluster.
- CLI, the command line interface which can be used to deploy and query Viziers.

This document outlines the process for setting up the development environment for each of these components.

## Setting up the Environment

Decide first if you'd like a full buildsystem (on a VM) or a containerized dev environment.

### VM as buildsystem

This utilizes `chef` to setup all dependencies and is based on `ubuntu`.
> [!Important]
>  The below description defaults to using a `minikube` on this VM for the developer to have an `all-in-one` setup. The VM type must support nested virtualization for `minikube` to work. Please confirm that the nested virtualization really is turned on before you continue, not all VM-types support it.
>  If you `bring-your-own-k8s`, you may disregard this.

```yaml
advancedMachineFeatures:
  enableNestedVirtualization: true
```

The following specifics were tested on GCP on a Ubuntu 24.04 (May 2025). Please see the latest [packer file](https://github.com/pixie-io/pixie/blob/main/tools/chef/Makefile#L56) for the current supported Ubuntu version: The initial compilation is CPU intense and `16vcpu` were a good trade-off, a balanced disk of 500 GB seems convenient and overall `n2-standard-16` works well.

> [!Warning]
>  The first `full build` takes several hours and at least 160 Gb of space
>  The first `vizier build` on these parameters takes approx. 1 hr and 45 Gb of space.





#### 1) Install chef and some dependencies

First, install `chef` to cook your `recipies`:

```bash
curl -L https://chefdownload-community.chef.io/install.sh | sudo bash
```
You may find it helpful to use a terminal manager like `screen` or `tmux`, esp to detach the builds.
```bash
sudo apt install -y screen git
```

In order to very significantly speed up your work, you may opt for a local cache directory. This can be shared between users of the VM, if both are part of the same group.
Create a cache dir under <directory-path> such as e.g. /tmp/bazel
```sh
sudo groupadd bazelcache
sudo usermod -aG bazelcache $USER
sudo mkdir -p <directory-path>
sudo chown -R :bazelcache <directory-path>
sudo chmod -R 2775 <directory-path>
```


Now, on this VM, clone pixie (or your fork of it)

```bash
git clone https://github.com/pixie-io/pixie.git
cd pixie/tools/chef
sudo chef-solo -c solo.rb -j node_workstation.json
sudo usermod -aG libvirt $USER
```

Make permanent the env loading via your bashrc
```sh
echo "source /opt/px_dev/pxenv.inc " >> ~/.bashrc
```


#### 2) If using cache, tell bazel about it

Edit the `<directory-path>` into the .bazelrc and put it into your homedir:
```
# Global bazelrc file, see https://docs.bazel.build/versions/master/guide.html#bazelrc.

# Use local Cache directory if building on a VM:
# On Chef VM, create a directory and comment in the following line:
 build --disk_cache=/tmp/bazel/ # Optional for multi-user cache: Make this directory owned by a group name e.g. "bazelcache"
```

```sh
cp .bazelrc ~/.
```

#### 3) Create/Use a registry you control and login

```sh
docker login ghcr.io/<myregistry>
```

#### 4) Prepare your kubernetes

> [!Important]
>  The below description defaults to using a `minikube` on this VM for the developer to have an `all-in-one` setup.
>  If you `bring-your-own-k8s`, please prepare your preferred setup and go to Step 5

If you added your user to the libvirt group (`sudo usermod -aG libvirt $USER`), starting the development environment on this VM will now work (if you did this interactively: you need to refresh your group membership, e.g. by logout/login). The following command will, amongst other things, start minikube
```sh
make dev-env-start
```

#### 5) Deploy a vanilla pixie

First deploy the upstream pixie (`vizier`, `kelvin` and `pem`) using the hosted cloud. Follow [these instructions](https://docs.px.dev/installing-pixie/install-schemes/cli) to install the `px` command line interface and Pixie:
```sh
px auth login
```

Once logged in to pixie, we found that limiting the memory is useful, thus after login, set the deploy option like so:
```sh
px deploy -p=1Gi
```
For reference and further information https://docs.px.dev/installing-pixie/install-guides/hosted-pixie/cosmic-cloud.

Optional on `minikube`:

You may encounter the following WARNING, which is related to the kernel headers missing on the minikube node (this is not your VM node). This is safe to ignore if Pixie starts up properly and your cluster is queryable from Pixie's [Live UI](https://docs.px.dev/using-pixie/using-live-ui). Please see [pixie-issue2051](https://github.com/pixie-io/pixie/issues/2051) for further details.
```
ERR: Detected missing kernel headers on your cluster's nodes. This may cause issues with the Pixie agent. Please install kernel headers on all nodes.
```

#### 6) Skaffold deploy your changes

Once you make changes to the source code, or switch to another source code version, use Skaffold to deploy (after you have the vanilla setup working on minikube)

Ensure that you have commented in the bazelcache-directory into the bazel config (see Step 2).


Review the compilation-mode suits your purposes:
```
cat skaffold/skaffold_vizier.yaml
# Note: You will want to stick with a sysroot based build (-p x86_64_sysroot or -p aarch64_sysroot),
# but you may want to change the --compilation_mode setting based on your needs.
# opt builds remove assert/debug checks, while dbg builds work with debuggers (gdb).
# See the bazel docs for more details https://bazel.build/docs/user-manual#compilation-mode
- name: x86_64_sysroot
  patches:
  - op: add
    path: /build/artifacts/context=./bazel/args
    value:
    - --config=x86_64_sysroot
    - --compilation_mode=dbg
#    - --compilation_mode=opt
```

Optional: you can make permanent your <default-repo> in the skaffold config:
```sh
skaffold config set default-repo <myregistry>
skaffold run -f skaffold/skaffold_vizier.yaml -p x86_64_sysroot
```

Check that your docker login token is still valid, then

```sh
skaffold run -f skaffold/skaffold_vizier.yaml -p x86_64_sysroot --default-repo=<myregistry>
```



#### 7) Golden Image

Once all the above is working and the first cache has been built, bake an image of your VM for safekeeping.




### Containerized Devenv
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
