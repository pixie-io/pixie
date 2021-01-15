# Profiling Stirling

As a developer of Stirling, it is often useful to profile Stirling to find performance bottlenecks.

### Profiling Locally

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
