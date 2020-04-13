#!/bin/bash -ex

PL_BUILD_TYPE=prod
PL_IMAGE_TAG=nightly-$(date +%s)-$(cat VERSION)
GIT_COMMIT=$(cat GIT_COMMIT)

export PL_BUILD_TYPE
export PL_IMAGE_TAG
export GIT_COMMIT

# This function generates the artifacts necessary for the cloud build and
# creates a manifest file that can be read by Spinnaker.
create_artifact() {
    artifact_name=$1
    skaffold_file=$2

    printenv

    # Build cloud assets.
    kustomize build k8s/cloud/staging > staging.yaml
    kustomize build k8s/cloud/prod > prod.yaml
    output_path="gs://pl-infra-dev-artifacts/kustomize/${BUILD_NUMBER}/${artifact_name}"
    gsutil cp staging.yaml "${output_path}/staging.yaml"
    gsutil cp prod.yaml "${output_path}/prod.yaml"

    skaffold build -q -o '{{json .}}' -f "${skaffold_file}" > manifest_internal.json

    # shellcheck disable=SC2002
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
            }' > "manifest_${artifact_name}.json"
}
