#!/bin/bash -ex
# create the CPUDistribution
bazel build //src/primitive_agent/bcc_agent:CPUDistribution_bcc
bazel build //src/platform_dependent/smoke_test:smoke_test

# copy over to the current directory
cp `bazel info workspace`/bazel-bin/src/primitive_agent/bcc_agent/CPUDistribution_bcc .
cp `bazel info workspace`/bazel-bin/src/platform_dependent/smoke_test/linux_amd64_static_pure_stripped/smoke_test .

# copy over bcc deb
gsutil cp gs://pl-infra-dev-artifacts/bcc-pixie-1.0.deb bcc-pixie-1.0.deb

# run the docker build that includes the stuff

# in the docker entrypoint edit the order to be run CPUDistribution then smoke-test
# add `-b -f $(pwd)/output.txt` as the arguments to smoke-test
echo "docker build"
docker build -t pl-smoke-test:v1.0 .

rm -f CPUDistribution_bcc
rm -f smoke_test
rm -f bcc-pixie-1.0.deb
