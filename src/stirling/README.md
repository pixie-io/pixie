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

## Profiling Stirling

### Locally

There's a script to automate profiling stirling on your local machine, using perf.

```
src/stirling/scripts/profile_stirling_wrapper.sh
```

### Profiling on GCE (including GKE nodes)

Perf doesn't run on GCE/GKE because the node is virtualized. Instead, we'll use a BPF based profiler.
See the link here for a basic overview: http://www.brendangregg.com/blog/2016-10-21/linux-efficient-profiler.html

[PEM resource usage](
https://work.withpixie.ai/live/clusters/gke_pl-pixies_us-west1-a_dev-cluster-stirling-perf/script?pem_resource_usage)
on `dev-cluster-stirling-perf`.

#### Dependencies

First, you'll need some prerequisite tools installed on the node:

```
sudo apt install linux-tools-common linux-tools-generic linux-tools-`uname -r`
sudo apt install bpfcc-tools linux-headers-$(uname -r)
sudo apt install gdb

sudo snap install --devmode bpftrace
sudo snap connect bpftrace:system-trace
```

There should now be a program called `/usr/sbin/profile-bpfcc` on your host machine.

Unfortunately, the program doesn't work out of the box, so we'll hack a few things to get it working.

#### Hacking the profiler

WARNING: The rest of this process is SUPER HACKY. I just don't have the patience to investigate the right fix yet.

First, lets create a copy.

```
sudo cp /usr/sbin/profile-bpfcc /usr/sbin/profile-bpfcc.pixie
```

Next edit the new file and make the following modifications.

1) Add the following to the top of the BPF code.
```
#ifdef asm_volatile_goto
#undef asm_volatile_goto
#define asm_volatile_goto(x...)
#endif
```

2) Hard-code `__PAGE_OFFSET_BASE`

To get the value of `__PAGE_OFFSET_BASE`, run this script:
```
sudo bpftrace -e 'BEGIN { $pob = kaddr("page_offset_base"); print($pob); }'

```

Finally, replace `__PAGE_OFFSET_BASE` with the value extracted from the bpftrace script above. Note that you should convert the value to hex first.

As long as the node doesn't reboot (which GKE nodes don't), the value of __PAGE_OFFSET_BASE shouldn't change and you should be okay.

#### Running the profiler

To run the profiler:

```
/usr/sbin/profile-bpfcc.pixie -p <target_pid> > profile.out
```

#### Creating a flamegraph graphic

Get the flamegraph repo and script flamegraph.pl, then follow these steps:
```
/usr/sbin/profile-bpfcc.pixie -p <target_pid> -f -F 101 300 > profile.out # 300 seconds, 101 Hz sampling, -f for flamegraph compat.
# gcloud compute scp & scp the file profile.out to your local machine, or eventual destination
flamegraph.pl profile.out > profile.svg
rsvg-convert profile.svg -f pdf > profile.pdf  # create pdf
rsvg-convert profile.svg -f eps > profile.eps  # create eps
convert -density 600 -quality 100 profile.eps profile.jpg  # create jpg based on eps
```

## BPF Tests

To test these programs, add tag `requires_bpf` to your test, and Jenkins will run these tests with
appropriate privileges and environment.

To run the tests locally, you can use `docker` or `sudo (i)bazel test`:

*   docker run -it --privileged --rm -v /sys:/sys -v /lib/modules:/lib/modules \
        -v /usr/src:/usr/src -v /home/{{user}}:/pl \
        gcr.io/pl-dev-infra/dev_image:201904181416 bash
    ibazel test //path/to:test --config=bpf
*   sudo ($which ibazel) test //path/to:test --config=bpf --strategy=TestRunner=standalone
    *   You need `--strategy=TestRunner=standalone` to disable bazel's own sandbox, which will
        prevent BPF to load the C code.
*   The `--config=bpf` reset the `--test_tag_filters` filter, so that the test can be selected.

