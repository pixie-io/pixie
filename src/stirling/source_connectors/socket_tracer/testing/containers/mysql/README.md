# MySQL Server image

This directory contains the build files for a python-based MySQL client in a container.
We use this client because it generates a more varied number of protocol messages than simpler clients.

This image is pre-built and made available via the BUILD.bazel file.

## Updating the image

If you need to update the container image, make the appropriate changes to the `Dockerfile`, bump the version in `update_ghcr.sh`, and then run `update_ghcr.sh` to build and upload the image.

You will then have to update `//bazel/pl_workspace.pl` with the updated SHA, which the script will print out for you.

## About the container

The container is built with a customized Python startup that prints the PID, since the test needs this information.

Finally, the entrypoint of the container takes, as an argument, a script to run. These can be volume-mounted to the /scripts directory.

See `mysql_container_bpf_test.cc` for use case.
