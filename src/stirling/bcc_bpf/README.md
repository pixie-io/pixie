# BPF C code

To test these programs, add tag `bpf` to your test, and Jenkins will run these tests with
appropriate privileges and environment.

To run the tests locally, you can use `docker` or `sudo bazel test`:

*   docker run -it --privileged --rm -v /sys:/sys -v /lib/modules:/lib/modules \
        -v /usr/src:/usr/src -v /home/{{user}}:/pl \
        gcr.io/pl-dev-infra/dev_image:201904181416 bash
    ibazel test //path/to:test --test_tag_filters=
*   sudo ibazel test //path/to:test --test_tag_filters=
*   The `--test_tag_filters=` reset the filters to empty, which was filtered out by the
    `//.bazelrc`>
