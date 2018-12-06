# Creating the Docker Image
Copy the smoke-test binary to //paltform-dependent/smoke-test/
Also download the bcc-pixie deb package using the following command:
```
gsutil cp gs://pl-infra-dev-artifacts/bcc-pixie-1.0.deb bcc-pixie-1.0.deb
```
Run the following command to build the docker image:
```
create_smoke_test.sh

docker tag pl-smoke-test:v1.0 gcr.io/pl-shared/smoke-test/pl-smoke-test:v1.0
docker push gcr.io/pl-shared/smoke-test/pl-smoke-test:v1.0
```

# Docker command to run the smoke-test

To run the smoke-test, do the following
We need to run the docker command with privileged mode for bcc to run.
```
mkdir pl_smoke_test
cd pl_smoke_test
docker run --privileged --pid=host -v $(pwd):/pl_mount -it pl-smoke-test:v1.0
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



# Managing permissions
```
# Providing permissions
./scripts/give_permissions.sh EMAIL
# Revoke permissions
./scripts/revoke_permissions.sh EMAIL
# See permissions
./scripts/see_permissions.sh
```
