#!/bin/bash

bazel run //src/stirling/source_connectors/jvm_stats/testing:push_hello_world_image

# kubectl apply does not force re-pulling the image, as the tag has not changed.
# We have to delete and then apply.
kubectl delete -f src/stirling/source_connectors/jvm_stats/testing/hello_world.yaml
kubectl apply -f src/stirling/source_connectors/jvm_stats/testing/hello_world.yaml
