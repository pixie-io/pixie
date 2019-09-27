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
