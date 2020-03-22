#!/usr/bin/env bash


copy_artifact_to_gcs() {
    output_path="$1"
    binary_path="$2"
    name="$3"

    sha256sum "${binary_path}" | awk '{print $1}' > sha
    gsutil -h 'Content-Disposition:filename=px' cp "${binary_path}" "${output_path}/${name}"
    gsutil cp sha "${output_path}/${name}.sha256"
    gsutil acl ch -u allUsers:READER "${output_path}/${name}"
    gsutil acl ch -u allUsers:READER "${output_path}/${name}.sha256"
}
