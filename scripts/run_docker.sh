#!/bin/bash -ex

function usage() {
  echo "run_docker.sh [--extra_args=<DEV_DOCKER_EXTRA_ARGS>]"
}

while [ "$1" != "" ]; do
    PARAM=`echo "$1" | awk -F= '{print $1}'`
    VALUE=`echo "$1" | awk -F= '{print $2}'`
    case $PARAM in
        -e | --extra_args)
            extra_args=$VALUE
            ;;
        *)
            echo "ERROR: unknown parameter \"$PARAM\""
            usage
            exit 1
            ;;
    esac
    shift
done

docker run --rm -it \
       -v ~/.config:/root/.config \
       -v "$HOME/.minikube:/root/.minikube" \
       -v "$HOME/.kube:/root/.kube" \
       -v /var/run/docker.sock:/var/run/docker.sock \
       -v "$HOME/.minikube:$HOME/.minikube" \
       -v "$GOPATH/src/pixielabs.ai:/pl/src/pixielabs.ai" \
       ${extra_args} \
       gcr.io/pl-dev-infra/dev_image_with_extras:201809261551 \
       bash
