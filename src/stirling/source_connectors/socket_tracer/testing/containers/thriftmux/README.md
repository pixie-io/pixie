# Thriftmux image

This directory contains the build files for running a thriftmux client and server.

This image is pre-built and made available via the BUILD.bazel file.

## Updating the image

If you need to update the container image, make the appropriate changes to the `Dockerfile`, bump the version in `update_gcr.sh`, and then run `update_gcr.sh` to build and upload the image.

You will then have to update `//bazel/container_images.bzl` with the updated SHA, which the script will print out for you.

## About the container

The container uses a multi-stage build where the first container installs `sbt` and other build dependencies. The final stage just copies the jars needed to run the application. This image provides both a thriftmux client and server. The docker container's default command will start the server process but this can be overriden. See the commands below for examples on how to use the image.

```bash
# Will run the server by default
$ docker run gcr.io/pixie-oss/pixie-dev-public/thriftmux
# Explicitly run the server
$ docker run gcr.io/pixie-oss/pixie-dev-public/thriftmux server

# Run the client instead of the server
$docker run gcr.io/pixie-oss/pixie-dev-public/thriftmux client
```

See `mux_container_bpf_test.cc` for use case.
