#!/bin/bash -ex

# shellcheck source=ci/artifact.sh
. "$(dirname "$0")/artifact.sh"


create_artifact "customer_docs" "skaffold/skaffold_customer_docs.yaml"
