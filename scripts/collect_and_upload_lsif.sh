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

popd
