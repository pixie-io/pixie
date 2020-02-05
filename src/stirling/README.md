# Stirling

Pixie's data collector on nodes. Stirling uses [eBPF](https://www.iovisor.org/technology/ebpf) to
snoop application metrics from inside Linux kernel. Particularly,
[BCC](https://github.com/iovisor/bcc) is used to write the BPF C code and manage the runtime.

## Testing Stirling on GKE with stirling_wrapper

`//src/stirling:stirling_wrapper` is a thin wrapper of Stirling's core library. It writes collected
data to STDIN, instead of exporting them into Carnot tables, which is the designated behavior.

`src/stirling/yaml/run_on_gke.sh` is a script to run `stirling_wrapper` on a GKE cluster.

## Testing PEM on GKE

After deploying Pixie on your test cluster, remove the deployed vizier:
`skaffold delete -f skaffold/skaffold_vizier.yaml`
And then redeploy it once again with your local changes:
`PL_BUILD_TYPE=dev skaffold run -f skaffold/skaffold_vizier.yaml`

## Stirling docker container environment

Stirling operates across kernel (through BPF), and user-space; and the user-space portion runs
inside container. Special cares are required to configure Stirling's docker container environment:

*   `--privileged`: As BPF requires root permission.
*   `--pid=host`: [OPTIONAL] Uses host's PID namespace. Used in tests to easily find the target PID
    and ensure it's same as what BPF sees. This is used inside `Jenkinsfile`.
*   `--volume=/:/host`: Make the entire host file system to `/host` inside container. Stirling needs
    this to access all of the data files on the host. One of them is the system headers, which is
    used by BCC to compile C code.
*   `--volume=/sys:/sys`: As BCC runtime requires to access this directory.

## JVM stats

JVM stats are derived from JVM perfdata file. JVM perfdata is a feature to export a set of
predefined JVM stats to a memory mapped file at `/tmp/hsperfdata_<user>/<pid>`. We call this file
`hsperfdata file`.

JVM perfdata is enabled by default in all mainstream JVMs, including the `Azul Zing JVM` used
by Hulu. Azul Zing JVM exports a few more stats, and uses different names for some stats
(usually with `sun` replaced with `azul`). `-XX:+PerfDisableSharedMem` or `-XX:-UsePerfData` can be
used to disable this feature.

The format of the hsperfdata file is not publicly documented. It was discovered by reading the
source code of `jstat` (the official JDK utility to read the file) and `hsstat` (a rewrite in
Golang). Our C++ APIs works same as `hsstat`. The major difference compared to jstat is
that our APIs only support 2 data types: `long` and `string`. This works because there is no known
mainstream JVM that produces other types of data.

After reading the raw data from the hsperfdata file, additional conversions are applied to derive
more meaningful stats. We follow `jstat`'s spec at [1]. Stat names are the same as `jstat`.
See [man page](https://docs.oracle.com/javase/7/docs/technotes/tools/share/jstat.html) for
explanation of each stat.

### Specific column

used_heap_size: Sum of all the fields that end with "U", see https://stackoverflow.com/a/44095604.
total_heap_size: Sum of all the fields that end with "C".
max_heap_size: Sum of all the fields that end with "MX".

[1] http://hg.openjdk.java.net/jdk8u/jdk8u/jdk/file/8c93eb3fa1c0/src/share/classes/sun/tools/jstat/resources/jstat_options
