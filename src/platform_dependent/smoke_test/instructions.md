# Setup

## Access
You will need access to two of our gcp resources
1. Our container registry for the Pixie labs smoke-test (`pl-shared`)
2. Our GCS storage bucket to drop off the test results (`pl-infra-shared`)

Send over your gcp profile's email address to either Phillip or Zain and one of them will whitelist you.

You will need to be logged into gcloud on the node. If you haven't authorized yet, run
```
gcloud auth login
```

## Software
You will need `docker` and [`gsutil`](https://cloud.google.com/storage/docs/gsutil_install)

# Running the smoke-test
1. ssh into a node on your staging cluster
2. Run the following commands
```
mkdir /tmp/pl_smoke_test
cd /tmp/pl_smoke_test
# run smoke test
docker run --privileged --pid=host -v $(pwd):/pl_mount -it gcr.io/pl-shared/smoke-test/pl-smoke-test:v1.0
# Send over the result package `smoke-test.tar`
gsutil cp smoke-test.tar gs://pl-infra-shared/smoke-test.tar
```

# Notes

## What we are testing
We're making sure that the tools we are using can work properly on your system.
Specifically, we need to be able to execute BCC/eBPF code on your machine to
ensure that our product will work on your specific configuration.
## Files sent over
This will output four files into the current directory
1. `smoke-test.log` : the log of all events that happened in the script
2. `smoke-test.bcc` : the output of one of our tools
3. `smoke-test.out` : the organized output of data we collected using the script
4. `smoke-test.tar` : the files above packaged into a tar file

We kept `smoke-test.<log|out|bcc>` so that you can inspect the content and make sure
that you're not sending over sensitive information.
