# Multi Platform Images

1. Install the tools

    ```bash
    sudo apt install binutils-aarch64-linux-gnu g++-12-aarch64-linux-gnu gcc-12-aarch64-linux-gnu
    sudo apt install qemu-user qemu-user-static
    ```

1. Proof of concept

    ```bash
    aarch64-linux-gnu-g++-12 -static -o aarch64_hello hello.cc
    qemu-aarch64 ./aarch64_hello
    ```

1. Setup docker buildx

    ```bash
    wget https://github.com/docker/buildx/releases/download/v0.10.0/buildx-v0.10.0.linux-amd64
    mkdir -p ~/.docker/cli-plugins/
    mv buildx-v0.10.0.linux-amd64 ~/.docker/cli-plugins/docker-buildx
    chmod +x ~/.docker/cli-plugins/docker-buildx

    docker run --privileged --rm tonistiigi/binfmt --install all

    docker buildx create --name mybuilder --driver docker-container --bootstrap
    docker buildx use mybuilder
    ```

1. Build

    ```bash
    docker buildx build --platform linux/amd64,linux/arm64 .
    ```

1. Push

    ```bash
    docker buildx build --platform linux/amd64,linux/arm64 -t gcr.io/pl-dev-infra/multi-arch-hello:latest --push .
    ```

1. Inspect

    ```bash
    docker buildx imagetools inspect gcr.io/pl-dev-infra/multi-arch-hello:latest
    ```

1. Test on k8s

    ```bash
    gcloud container clusters get-credentials multi-arch --zone us-central1-a --project pl-pixies
    kubectl create ns hello
    kubectl apply -f hello.yaml
    ```

## DONE 🎉
