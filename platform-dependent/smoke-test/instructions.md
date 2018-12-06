# Setup
You will need access to two gcp resources
1. Our container registry for the Pixie labs smoke-test.
2. Our GCS storage bucket to drop off the test results

Send your gcp user email to either Phillip or Zain and he'll whitelist you
# Docker command to run the smoke-test

To run the smoke-test, do the following
We need to run the docker command with privileged mode for bcc to run.
```
mkdir pl_smoke_test
cd pl_smoke_test
docker run --privileged --pid=host -v $(pwd):/pl_mount -it gcr.io/pl-shared/smoke-test/pl-smoke-test:v1.0
```

This will output three files into the current directory
1. `smoke-test.log` : the log of all events that happened in the script
2. `smoke-test.out` : the output of data we collected using the script
3. `smoke-test.tar` : the packaged up tar of the above files.

Send over the `smoke-test.tar` by running
```
gsutil cp smoke-test.tar gs://pl-infra-shared/smoke-test.tar
```

We kept `smoke-test.<log|out>` so that you can inspect the content and make sure
that you're not sending over sensitive information.
