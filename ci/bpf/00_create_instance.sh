#!/bin/bash -ex

# Copyright 2018- The Pixie Authors.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# SPDX-License-Identifier: Apache-2.0

if [[ -z ${GCP_PROJECT} ]]; then
  echo "GCP_PROJECT must be set";
  exit 1
fi

DEFAULT_IMAGE="projects/${GCP_PROJECT}/global/images/bpf-runner-4-14-215"
IMAGE_TO_USE=""

if [[ $KERNEL_VERSION = '5.19' ]]; then
  IMAGE_TO_USE="projects/${GCP_PROJECT}/global/images/bpf-runner-5-19-11"
elif [[ $KERNEL_VERSION = '5.15' ]]; then
  IMAGE_TO_USE="projects/${GCP_PROJECT}/global/images/bpf-runner-5-15-15"
elif [[ $KERNEL_VERSION = '5.10' ]]; then
  IMAGE_TO_USE="projects/${GCP_PROJECT}/global/images/bpf-runner-5-10-149"
elif [[ $KERNEL_VERSION = '5.4' ]]; then
  IMAGE_TO_USE="projects/${GCP_PROJECT}/global/images/bpf-runner-5-4-19"
elif [[ $KERNEL_VERSION = '4.19' ]]; then
  IMAGE_TO_USE="projects/${GCP_PROJECT}/global/images/bpf-runner-4-19-260"
elif [[ $KERNEL_VERSION = '4.14' ]]; then
  IMAGE_TO_USE="projects/${GCP_PROJECT}/global/images/bpf-runner-4-14-215"
elif [[ $KERNEL_VERSION ]]; then
  echo "Unsupported kernel version: ${KERNEL_VERSION}"
  exit 1
else
  IMAGE_TO_USE="${DEFAULT_IMAGE}"
fi

NAME="bpf-${KERNEL_VERSION//./-}-${BUILD_TAG/jenkins-/}"

printenv

gcloud components install beta --quiet

gcloud beta compute instances create \
  "${NAME}" \
  --quiet \
  --project="${GCP_PROJECT}" \
  --zone=us-west1-b \
  --machine-type=n2d-standard-32 \
  --service-account="jenkins-worker@${GCP_PROJECT}.iam.gserviceaccount.com" \
  --scopes=https://www.googleapis.com/auth/cloud-platform \
  --network-interface=network-tier=PREMIUM,subnet=us-west1-0 \
  --maintenance-policy=MIGRATE --provisioning-model=STANDARD \
  --instance-termination-action=DELETE --max-run-duration=10800s \
  --service-account="jenkins-worker@${GCP_PROJECT}.iam.gserviceaccount.com" \
  --scopes=https://www.googleapis.com/auth/cloud-platform \
  --create-disk="auto-delete=yes,boot=yes,device-name=instance-1,image=${IMAGE_TO_USE},mode=rw,size=256,type=projects/${GCP_PROJECT}/zones/us-central1-a/diskTypes/pd-ssd"

cleanup() {
  gcloud compute instances delete \
  "${NAME}" \
  --project="${GCP_PROJECT}" \
  --zone=us-west1-b \
  --delete-disks=all \
  --quiet
}
trap cleanup EXIT

run_on_instance() {
  PASSTHROUGH_ENV=(
    "BUILDABLE_FILE=${BUILDABLE_FILE}"
    "TEST_FILE=${TEST_FILE}"
    "BAZEL_ARGS='${BAZEL_ARGS}'"
    "STASH_NAME=${STASH_NAME}"
    "GCS_STASH_BUCKET=${GCS_STASH_BUCKET}"
    "BUILD_TAG=${BUILD_TAG}"
    "BES_FILE=${BES_FILE}"
  )

  gcloud compute ssh \
    "jenkins@${NAME}" \
    --project="${GCP_PROJECT}" \
    --zone=us-west1-b \
    --command="${PASSTHROUGH_ENV[*]} $*"
}

scp_to_instance() {
  gcloud compute scp \
    --project="${GCP_PROJECT}" \
    --zone=us-west1-b \
    "$@"
}

MAX_SEC=300
TIMEOUT=$(($(date -u +%s) + MAX_SEC))

until run_on_instance ls; do
  if [[ $(date -u +%s) -gt TIMEOUT ]]; then
    echo "Timed out waiting for ssh propagation"
    exit 1;
  fi
  echo "waiting for ssh propagation"
  sleep 2;
done;

scp_to_instance ./.archive/src.tar.gz "jenkins@${NAME}":src.tar.gz
scp_to_instance ./.archive/targets.tar.gz "jenkins@${NAME}":targets.tar.gz

run_on_instance tar -zxf src.tar.gz --no-same-owner
run_on_instance tar -zxf targets.tar.gz --no-same-owner

run_on_instance ./ci/bpf/01_setup_instance.sh
run_on_instance ./ci/bpf/02_docker_run.sh
