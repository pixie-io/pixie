#!/bin/bash -ex

# This script generates the artifacts necessary for the cloud build and
# creates a manifest file that can be read by Spinnaker.

printenv

PL_BUILD_TYPE=prod
PL_IMAGE_TAG=nightly-$(date +%s)-`cat SOURCE_VERSION`
GIT_COMMIT=`cat GIT_COMMIT`

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
