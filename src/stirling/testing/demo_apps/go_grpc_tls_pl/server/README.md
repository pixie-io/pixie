# Go GRPC and HTTP2 server for testing HTTP2/GRPC traicing

This directory contains a Go grpc and http2 server for testing Pixie's Go http2 and grpc tracing. This application is built through bazel and by the `update_ghcr.sh` script contained in this directory. The reason for this is that as Go versions fall out of support, maintaining these in our bazel build hinders our ability to upgrade our go deps and to upgrade Pixie's Go version.

In addition to this, Pixie's upcoming opentelemetry-go-instrumentation offsetgen based tracing requires building binaries with Go's toolchain until https://github.com/bazel-contrib/rules_go/issues/3090 is resolved.

As new Go versions are released, the out of support versions should be removed from bazel and added to the `update_ghcr.sh` script in this directory. This will allow our builds to maintain test coverage for older Go versions without complicating our ability to upgrade Pixie's Go version and dependencies.

Run `update_ghcr.sh` in this directory to push the images for each Go version to the ghcr.io repo.
