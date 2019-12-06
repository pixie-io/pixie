#!/usr/bin/env bash
# Script to generate certs and place them into yaml files.
workspace=$(bazel info workspace 2> /dev/null)
curdirectory=${workspace}/src/cloud/dnsmgr/scripts

if [ $# -ne 3 ]; then
  echo "Expected 3 Arguments: EMAIL and OUTDIR and NUMCERTS. Received $#."
  exit 1
fi
EMAIL=$1
OUTDIR=$2
NUMCERTS=$3
"${curdirectory}"/generate_certs.sh "${EMAIL}" "${OUTDIR}" "${NUMCERTS}"
"${curdirectory}"/convert_certs_to_yaml.sh "${OUTDIR}"/certificates ./certs
