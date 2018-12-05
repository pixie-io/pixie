# Creating the Docker Image
Copy the smoke-test binary to //paltform-dependent/smoke-test/
Also download the bcc-pixie deb package using the following command:
```
gsutil cp gs://pl-infra-dev-artifacts/bcc-pixie-1.0.deb bcc-pixie-1.0.deb
```
Run the following command to build the docker image:
```
docker build -t pl-smoke-test:v1.0 .
```

# Docker command to run the smoke-test

We need to run the docker command with privileged mode for bcc to run.
```
docker run --privileged --pid=host -it pl-smoke-test:v1.0
```
