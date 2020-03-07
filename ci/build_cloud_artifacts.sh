#!/bin/bash -ex

# This script generates the artifacts necessary for the cloud build and
# creates a manifest file that can be read by Spinnaker.

printenv

PL_BUILD_TYPE=prod
PL_IMAGE_TAG=nightly-$(date +%s)-$(cat VERSION)
GIT_COMMIT=$(cat GIT_COMMIT)

export PL_BUILD_TYPE
export PL_IMAGE_TAG
export GIT_COMMIT

# Build Kustomize.
kustomize build k8s/cloud/staging > staging.yaml
kustomize build k8s/cloud/prod > prod.yaml
output_path="gs://pl-infra-dev-artifacts/kustomize/${BUILD_NUMBER}"
gsutil cp staging.yaml "${output_path}/staging.yaml"
gsutil cp prod.yaml "${output_path}/prod.yaml"

skaffold build -q -o '{{json .}}' -f skaffold/skaffold_cloud.yaml > manifest_internal.json

cat manifest_internal.json | \
    jq '[[.builds[] | . + {"name": "docker/image"}]]' | \
    jq \
        --arg messageFormat jenkinsArtifact2 \
        --arg buildID "${BUILD_NUMBER}" \
        --arg jobName "${JOB_NAME}" \
        --arg gitCommit "${GIT_COMMIT}" \
        '{
             "artifacts": .[],
             "build_project": $jobName,
             "build_id":      $buildID,
             "gitCommit":     $gitCommit,
             "messageFormat": "jenkinsArtifact2",
             "customFormat": true
         }' > manifest.json
