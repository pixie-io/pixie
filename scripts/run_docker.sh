#!/bin/bash -ex

function usage() {
  echo "run_docker.sh [--extra_args=<DEV_DOCKER_EXTRA_ARGS>]"
}

# Read variables from docker.properties file.
dockerPropertiesFile="$GOPATH/src/pixielabs.ai/pixielabs/docker.properties"
if [ -f "$dockerPropertiesFile" ]
then
  while IFS='=' read -r key value
  do
    eval ${key}=\${value}
  done < "$dockerPropertiesFile"
else
  echo "$dockerPropertiesFile not found."
fi

# Parse arguments.
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
       --network host \
       -v ~/.config:/root/.config \
       -v "$HOME/.ssh:/root/.ssh" \
       -v "$HOME/.minikube:/root/.minikube" \
       -v "$HOME/.kube:/root/.kube" \
       -v "$HOME/.gitconfig:/root/.gitconfig" \
       -v "$HOME/.arcrc:/root/.arcrc" \
       -v /var/run/docker.sock:/var/run/docker.sock \
       -v /var/lib/docker:/var/lib/docker \
       -v "$HOME/.minikube:$HOME/.minikube" \
       --pid=host -v /:/host -v /sys:/sys --env PL_HOST_PATH=/host \
       -v "$GOPATH/src/pixielabs.ai:/pl/src/pixielabs.ai" \
       ${extra_args} \
       "gcr.io/pl-dev-infra/dev_image_with_extras:$DOCKER_IMAGE_TAG" \
       bash
