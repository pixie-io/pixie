# Development Environment

The Pixie project currently consists of three main components:

- Pixie Cloud, the control plane of Pixie responsible for managing Viziers and hosting the UI.
- Vizier, the data plane of Pixie containing the agents deployed to a K8s cluster.
- CLI, the command line interface which can be used to deploy and query Viziers.

This document outlines the process for setting up the development environment for each of these components.

## Setting up the Environment

Decide first if you'd like a full buildsystem (on a VM) or a containerized dev environment.

### VM as buildsystem

This utilizes `chef` to setup all dependencies and is based on `ubuntu`. The VM type must support nested virtualization for `minikube` to work.


The following specifics were tested on GCP on a Ubuntu 24.04 (May 2025): The initial compilation is CPU intense and `16vcpu` were a good trade-off, a balanced disk of 500 GB seems convienent and overall `n2-standard-16` works well. 

> [!Warning]
>  The first build takes several hours and at least 160 Gb of space
> Turn on nested virtualization during provisioning and avoid the use of `spot` VMs for the first build to avoid the very long first build interrupting. If you create the VMs as templates from an image, you can later switch to more cost-effective `spot` instances.



```yaml
advancedMachineFeatures:
  enableNestedVirtualization: true
```

1) Install chef and some dependencies

WIP: this needs to be retested after moving it into `chef` rather than doing by hand or via init-script:
While we re-test, you may run the following install manually 
```bash
sudo apt update 
sudo apt install -y git coreutils mkcert libnss3-tools libvirt-daemon-system libvirt-clients qemu-kvm virt-manager
```


```bash
curl -L https://chefdownload-community.chef.io/install.sh | sudo bash
```
You may find it helpful to use a terminal manager like `screen` or `tmux`, esp to detach the builds.
```bash
sudo apt install -y screen
```
Now, on this VM, clone pixie (or your fork of it)

```
git clone https://github.com/pixie-io/pixie.git
cd pixie/tools/chef
sudo chef-solo -c solo.rb -j node_workstation.json
sudo usermod -aG libvirt $USER
```

Make permanent the env loading via your bashrc
```sh
echo "source /opt/px_dev/pxenv.inc " >> ~/.bashrc
```

Put the baselrc into your homedir:
```sh
cp .bazelrc ~/.
```
In order to very significantly speed up your work, you may opt for a local cache directory. This can be shared between users of the VM, if both are part of the same group.
Create a cache dir under <directory-path> like /tmp/bazel
```sh
sudo groupadd bazelcache
sudo usermod -aG bazelcache $USER
sudo mkdir -p <directory-path>
sudo chown :bazelcache <directory-path>
sudo chmod 2775 <directory-path>
```

2) Create/Use a registry you control and login
   
```sh
docker login ghcr.io/<myregistry>
```

3) Make Minikube run and deploy a vanilla pixie

If you added your user to the libvirt group (`sudo usermod -aG libvirt $USER`), starting the development environment on this VM will now work (if you did this interactively: you need to refresh your group membership, e.g. by logout/login). The following command will, amongst other things, start minikube
```sh
make dev-env-start
```

Onto this minikube, we first deploy the upstream pixie (`vizier`, `kelvin` and `pem`) using the remote cloud  `export PX_CLOUD_ADDR=getcosmic.ai` . Follow https://docs.px.dev/installing-pixie/install-schemes/cli , to install the  `px` command line interface and login:
```sh
px auth login
```

Once logged in to pixie, we found that limiting the memory is useful, thus after login, set the deploy option like so:
```sh
px deploy -p=1Gi
```
For reference and further information https://docs.px.dev/installing-pixie/install-guides/hosted-pixie/cosmic-cloud

4) Once you make changes to the source code, or switch to another source code version, use Skaffold to deploy (after you have the vanilla setup working on minikube)
 
Check that your docker login token is still valid, then

```sh
> skaffold run -f skaffold/skaffold_vizier.yaml -p x86_64_sysroot --default-repo=ghcr.io/<myregistry>
```

Optional: you can set default-repo on config, so that you don't need to pass it as an argument everytime
```sh
> skaffold config set default-repo ghcr.io/<myregistry> 
> skaffold run -f skaffold/skaffold_vizier.yaml -p x86_64_sysroot
```

5) Golden image

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
