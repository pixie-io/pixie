#!/bin/bash

workspace=$(bazel info workspace 2>/dev/null)

bazel build --bes_backend="" --bes_results_url="" @com_github_bazelbuild_buildtools//buildifier:buildifier 2>/dev/null 1>/dev/null
bazel-bin/external/com_github_bazelbuild_buildtools/buildifier/buildifier_/buildifier \
--lint=warn \
--warnings=+out-of-order-load,+unsorted-dict-items,+native-cc,+native-java,+native-proto,+native-py,-module-docstring,-function-docstring,-function-docstring-args,-function-docstring-header \
--path="${workspace}" "${workspace}/$1" 2>&1 || true