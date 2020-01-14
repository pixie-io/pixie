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
