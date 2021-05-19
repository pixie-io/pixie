# Stirling

Stirling is Pixie's data collector on nodes. Stirling uses Linux APIs, including [eBPF technology](https://www.iovisor.org/technology/ebpf) to gather application metrics and events from the Linux kernel, system libraries, or the application itself.

Some of the primary data collected by Stirling includes:

- Application CPU, memory and network utilization.
- Application network messaging events for a variety of protocols (e.g. HTTP, MySQL, Postgres, ...).

The Stirling architecture consists of a core infrastructure into which "source connectors" can be added to extract information from different data sources. The source connectors export data tables which are published via the Stirling API.

## Directory Structure

The Stirling directory structure is as follows:

```
binaries            # Standalone executable versions of Stirling.
bpf_tools           # Tools for managing the compilation and deployment of BPF code.
BUILD.bazel         # Top-level build file.
core                # Core Stirling infrastructure, without data source connectors.
e2e_tests           # Top-level stirling tests (mostly shell tests).
LICENSE.txt         # Stirling specific license
obj_tools           # Tools for managing binary objects (e.g. ELF and DWARF information).
proto               # Public messages for publishing data tables.
README.md           # This README.
scripts             # Utility scripts.
source_connectors   # Directory containing all source connectors, which are responsible for gathering data.
stirling.cc         # Top-level Stirling source code.
stirling.h          # Top-level Stirling header file.
testing             # Testing utilities.
utils               # Utilities.
```

### BPF code

Many of the `source_connectors` use BPF (managed by BCC) to collect their data.

We use [BCC](https://github.com/iovisor/bcc) to write the BPF C code and manage the runtime of the source connectors that use BPF.

For each of these BPF based `source_connectors`, there should be at least two subdirectories:

- `bcc_bpf`: Contains all the BPF code that is compiled by BCC. This code is licensed differently than the rest of Stirling and is intentionally kept isolated. No code outside the directory should `#include` or link to the code in this directory. All data transfers are done through BPF maps or buffers, data structures for which are described in `bcc_bpf_intf`.
- `bcc_bpf_intf`: Contains public data structures that are used to transport data out from the kernel to user-space. The headers in this file are typically `#include`d by both the BPF code and our user-space C++ code.

## Building & Testing

The Stirling code base can be build with the following command:

```
bazel build //src/stirling/...
```

Certain Stirling tests use BPF, which requires root privileges.
Those tests are marked with the bazel tag `requires_bpf`.
When you run the standard `bazel test`, only those tests that do not require BPF are run.
```
bazel test //src/stirling/...
```

To run an individual BPF test, you can use the `sudo_bazel_run.sh` convenience script. For example:
```
sudo_bazel_run.sh //src/stirling/source_connectors/socket_tracer:socket_trace_bpf_test

```
The `sudo_bazel_run.sh` builds the target, and then uses sudo to run the script directory.

Note: if a test uses sharding, `--test_sharding_strategy=disabled` is required to run the test.

### Running the BPF test suite

To run the entire BPF test suite, you can deploy to Jenkins, which will run these tests with the appropriate priveleges and environment.

To run the tests locally, you can also use a container:

```
$PIXIE_ROOT/scripts/run_docker.sh

bazel test --config=bpf //path/to:test
```

## Stirling Wrapper

`stirling_wrapper` is a stand-alone version of Stirling that one can run locally. It does not require the rest of the PEM, or even k8s. It will collect the data from its sources and print the results directly to STDOUT.

You can run `stirling_wrapper` via the following command, which builds `stirling_wrapper` and runs the executable with a sudo prompt.
```
$PIXIE_ROOT/src/stirling/scripts/stirling_wrapper.sh
```

## Stirling docker container environment

Because Stirling uses the Linux kernel APIs (including BPF), special flags are required to make sure Stirling runs properly when Stirling is itself inside a container. Since the kernel is not part of the container, Stirling's view is that it should be tracing the host. For example, the BPF probes will naturally be on the host and not confined to the container.

To make sure Stirling runs properly in a container environment, the following flags are required:

*   `--privileged`: BPF requires root permissions.
*   `--volume=/sys:/sys`: Required for BPF probes to function correctly.
*   `--pid=host`: Uses host's PID namespace. Without this, Stirling's calls to `getpid()` won't be correctly resolved.
    Also used in tests to easily find the target PID and ensure it's same as what BPF sees. This is used inside `Jenkinsfile`.
*   `--volume=/:/host`: Make the entire host file system to `/host` inside container. Stirling needs
    this to access all of the data files on the host. One of them is the system headers, which is
    used by BCC to compile C code. Also required for accessing debug info of binaries in other containers.
*   `--env PL_HOST_PATH=/host`: Related to the line above.

If any additional flags become required, be sure to update at least the following files:
 - `scripts/run_docker.sh`
 - `k8s/pem/pem_daemonset.yaml`
 - `src/stirling/k8s`: all yaml files
 - `src/stirling/scripts/docker_run_stirling_wrapper.sh`
 - `src/stirling/e2e_tests`: files that run stirling in a container

## Dynamically installed Linux headers in Stirling's BPF runtime

Stirling's BPF runtime requires kernel headers to compile the BPF code. If the target host does not
have kernel headers pre-installed, Stirling looks for the appropriate headers package bundled with
the PEM image, and extracts them to the correct path inside the container.

In order to limit the size of the PEM image, only a subset of kernel versions are included.
Stirling picks the headers package with the closest version number to the host's kernel version.

As a result, the dynamically-installed kernel headers might not exactly match the host kernel. This
may result in unexpected runtime behavior if certain structs are not the right match.  For instance,
in places where Stirling's BPF code accesses kernel data structures, an accessed member's offset in
a struct may have changed between versions, and the byte code compiled with an older version of the
kernel headers would result in accessing wrong data.  More rarely, the BPF code compilation may fail
altogether if struct member names have changed.

To avoid such issues, we manually examine the history of the kernel data structures accessed in
Stirling's BPF code, and make sure the correct linux headers version are included in the PEM image.

## FAQ

### Why do some of my records have exactly the same latency?

Rarely, a few successive records may have the exactly-same latency value. That is because theose
records, despite being separated into multiple req/resp pairs, were actually sent in batch by
syscalls. Stirling captures data from those syscalls, and cannot assign different timestamps to
those messages. As a result, records have the exactly-same latency.

This is an expected behavior, and a result of Stirling's use of eBPF kprobes.
