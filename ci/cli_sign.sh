#!/usr/bin/env bash

set -ex

printenv

#TODO(zasgar): Try to make it use another keychain, but it's not a big deal either way
# since this machine is not used for anythign else.
security unlock-keychain -p "$JENKINSKEY" login.keychain

release_tag=${TAG_NAME##*/v}
ARTIFACT_BASE_PATH=https://storage.googleapis.com/pixie-prod-artifacts/cli
url="${ARTIFACT_BASE_PATH}/${release_tag}/cli_darwin_amd64_unsigned"

rm -f cli_darwin_amd64_unsigned
wget "${url}"

mv cli_darwin_amd64_unsigned cli_darwin_amd64

gon ci/gon.hcl
