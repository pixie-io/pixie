#!/bin/bash -ex

SOURCEGRAPH_ENDPOINT="https://cs.corp.pixielabs.ai"

GIT_COMMIT=""
SOURCEGRAPH_ACCESS_TOKEN=""

# Print out the usage information and exit.
usage() {
  echo "Usage $0 [-t <sourcegraph_token>] [-c <git_commit>]" 1>&2;
  exit 1;
}

parse_args() {
  local OPTIND
  # Process the command line arguments.
  while getopts "c:t:h" opt; do
    case ${opt} in
      c)
        GIT_COMMIT=$OPTARG
        ;;
      t)
        SOURCEGRAPH_ACCESS_TOKEN=$OPTARG
        ;;
      :)
        echo "Invalid option: $OPTARG requires an argument" 1>&2
        ;;
      h)
        usage
        ;;
      *)
        usage
        ;;
    esac
  done
  shift $((OPTIND -1))
}

# Parse the input arguments.
parse_args "$@"

export SRC_ENDPOINT="${SOURCEGRAPH_ENDPOINT}"
export SRC_ACCESS_TOKEN="${SOURCEGRAPH_ACCESS_TOKEN}"

pushd "$(bazel info workspace)"

LSIF_GO_OUT="go.dump.lsif"

/opt/pixielabs/bin/lsif-go \
  --verbose \
  --no-animation \
  --output="${LSIF_GO_OUT}"

/opt/pixielabs/bin/src lsif upload \
  -repo=github.com/pixie-labs/pixielabs \
  -file="${LSIF_GO_OUT}" \
  -commit="${GIT_COMMIT}" \
  -ignore-upload-failure \
  -no-progress

LSIF_TS_OUT="ts.dump.lsif"

lsif-tsc --out "${LSIF_TS_OUT}" -p src/ui

/opt/pixielabs/bin/src lsif upload \
  -repo=github.com/pixie-labs/pixielabs \
  -file="${LSIF_TS_OUT}" \
  -commit="${GIT_COMMIT}" \
  -ignore-upload-failure \
  -no-progress

LSIF_CPP_OUT="cpp.dump.lsif"

./scripts/gen_compilation_database.py \
  --run_bazel_build \
  --include_genfiles \
  "$(bazel query 'kind("cc_(binary|test) rule",//... -//third_party/... -//demos/... -//experimental/...) except attr("tags", "manual", //...)')"

lsif-clang \
  --extra-arg="-resource-dir=$(clang -print-resource-dir)" \
  --out="${LSIF_CPP_OUT}" \
  compile_commands.json

/opt/pixielabs/bin/src lsif upload \
  -repo=github.com/pixie-labs/pixielabs \
  -file="${LSIF_CPP_OUT}" \
  -commit="${GIT_COMMIT}" \
  -ignore-upload-failure \
  -no-progress

popd
