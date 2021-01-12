# BPF C code

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


## Data alignment in perf buffers

Perf buffers are 8-bytes-aligned [1]. But each record submitted to perf buffers has a 4-bytes size
field, followed by the actual data. Therefore, when reading from perf buffers, the data is always
4-bytes aligned.

If the data type copied from perf buffers are not having the same alignment as 4-bytes,
a naive pointer cast might cause runtime error in ASAN. In rare situations, when compiling on
non-X86 CPUs, compilers might produce misaligned, therefore inefficient, code.

## references

[1] https://github.com/iovisor/bcc/issues/2432#issuecomment-506962238
