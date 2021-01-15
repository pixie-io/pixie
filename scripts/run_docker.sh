#!/bin/bash -ex

function usage() {
  echo "run_docker.sh [--extra_args=<DEV_DOCKER_EXTRA_ARGS>]"
}

# Read variables from docker.properties file.
dockerPropertiesFile="docker.properties"
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

# See src/stirling/README.md, under "Stirling docker container environment" for
# an explanation of Stirling requirements.
stirling_flags="--privileged
                -v /:/host
                -v /sys:/sys
                -v /var/lib/docker:/var/lib/docker
                --pid=host
                --env PL_HOST_PATH=/host"

# Disable quoting check to use stirling_flags, otherwise the flag values are treated as one string.
# shellcheck disable=SC2086
docker run --rm -it \
       --network=host \
       ${stirling_flags} \
       -v /var/run/docker.sock:/var/run/docker.sock \
       -v "$HOME/.config:/root/.config" \
       -v "$HOME/.ssh:/root/.ssh" \
       -v "$HOME/.minikube:/root/.minikube" \
       -v "$HOME/.minikube:$HOME/.minikube" \
       -v "$HOME/.kube:/root/.kube" \
       -v "$HOME/.gitconfig:/root/.gitconfig" \
       -v "$HOME/.arcrc:/root/.arcrc" \
       -v "$GOPATH/src/pixielabs.ai:/pl/src/pixielabs.ai" \
       ${extra_args} \
       "gcr.io/pl-dev-infra/dev_image_with_extras:$DOCKER_IMAGE_TAG" \
       bash
