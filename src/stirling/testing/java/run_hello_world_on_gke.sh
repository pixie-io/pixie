#!/bin/bash

bazel run //src/stirling/testing/java:push_hello_world_image

# Because simply apply the yaml file would not force re-pulling the image, as the tag has not been
# changed. We have to delete and then apply.
# We can use kubectl rollout restart:
# https://stackoverflow.com/a/51835397
kubectl delete -f src/stirling/testing/java/hello_world.yaml
kubectl apply -f src/stirling/testing/java/hello_world.yaml
