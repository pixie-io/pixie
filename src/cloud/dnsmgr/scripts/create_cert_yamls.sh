#!/usr/bin/env bash
# Script to generate certs and place them into yaml files.
workspace=$(bazel info workspace 2> /dev/null)

EMAIL="philkuz@pixielabs.ai"
OUTDIR="./.lego"
${workspace}/scripts/generate_certs.sh $EMAIL ${OUTDIR}
${workspace}/scripts/convert_certs_to_yaml.sh ${OUTDIR}/certificates ./certs
