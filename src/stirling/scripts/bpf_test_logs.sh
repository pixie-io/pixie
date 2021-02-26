#!/bin/bash -e

if (( $# < 1 )); then
  echo "$0 <build_id>"
  echo "Fetch the test log archive of the BPF test from GCS."
  echo "Extract it and remove the original archive file."
  echo "Build ID can be found from the Jenkins page of the test build."
  echo "Look for the number at the upper left corner of the test summary page."
  exit 1
fi

# TODO(yzhao): We can create a common library to generate GCS path, to be used by tests.
# So we can have a pixie-wide test log fetching script. ATM, the schema is not consistent across
# tests.

workflow_name="pixie-dev-main-phab-test"
build_id=$1
gs_url="gs://px-jenkins-build-temp/jenkins-${workflow_name}-${build_id}/build-bpf-testlogs.tar.gz"
local_file=$(basename "${gs_url}")
extract_dir=$(echo "${local_file}" | cut -f 1 -d '.')

gsutil cp "${gs_url}" "${local_file}"

mkdir -p "${extract_dir}"
echo "Logs are extracted to ./${extract_dir}, it might be ignored by git ..."
tar -zxf "${local_file}" --directory "${extract_dir}"
rm -f "${local_file}"
