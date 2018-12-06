#!/bin/bash -x
# create the CPUDistribution
bazel build //primitive-agent/bcc_agent:CPUDistribution_bcc
bazel build //platform-dependent/smoke-test:smoke-test

# copy over to the current directory
cp `bazel info workspace`/bazel-bin/primitive-agent/bcc_agent/CPUDistribution_bcc .
cp `bazel info workspace`/bazel-bin/platform-dependent/smoke-test/linux_amd64_static_pure_stripped/smoke-test .

# copy over bcc deb
gsutil cp gs://pl-infra-dev-artifacts/bcc-pixie-1.0.deb bcc-pixie-1.0.deb

# run the docker build that includes the stuff

# in the docker entrypoint edit the order to be run CPUDistribution then smoke-test
# add `-b -f $(pwd)/output.txt` as the arguments to smoke-test
echo "docker build"
docker build -t pl-smoke-test:v1.0 .

rm -f CPUDistribution_bcc
rm -f smoke-test
