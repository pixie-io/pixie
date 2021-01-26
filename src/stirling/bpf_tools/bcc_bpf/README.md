# BPF C Code

## Data alignment in perf buffers

Perf buffers are 8-bytes-aligned [1]. But each record submitted to perf buffers has a 4-bytes size
field, followed by the actual data. Therefore, when reading from perf buffers, the data is always
4-bytes aligned.

If the data type copied from perf buffers are not having the same alignment as 4-bytes,
a naive pointer cast might cause runtime error in ASAN. In rare situations, when compiling on
non-X86 CPUs, compilers might produce misaligned, therefore inefficient, code.

## Debugging

`bpf_trace_printk()` is the equivalent of `printf()` in BCC. The output can be viewed with:
`sudo cat /sys/kernel/debug/tracing/trace_pipe`.

You can insert `__LINE__` in `bpf_trace_printk()` to improve the readability of the output.

## references

[1] https://github.com/iovisor/bcc/issues/2432#issuecomment-506962238
