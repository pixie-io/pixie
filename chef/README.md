# Machine provisioning configs

[Link to doc](https://phab.pixielabs.ai/w/eng/machine_provisioning/)

## BCC/BPF build or runtime failures

Progressively, try the follow procedure if you have encountered build or runtime failure in code
involving BCC/BPF code (mostly Stirling).

* `bazel clean`: Remove bazel output.
* `bazel clean --expunge; bazel shutdown`: In addition to the above, shutdown server and restart bazel.
* `sudo rm -rf ~/.cache/bazel`: Remove the entire bazel output directory.
