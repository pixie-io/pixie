# This macro preprocesses BPF C files to include any user-includes.
# It essentially does this by running the C preprocessor.
# BPF source should not, however, have system includes inlined, as that
# should be done on the host system where the BPF code is deployed.
# There is, unfortunately no way to tell the C preprocessor to process user includes only.
# To fake this, there is a set of mock system headers that is defined,
# which serve as an indirection so the includes stay in place.
# The mock system headers take the following form:
# file: linux/sched.h
# contents: include <linux/sched.h>
#
# @param src: The location of the BPF source code.
# @param hdrs: Any user defined headers required by the BPF program (src).
# @param syshdrs: The location of the fake system headers to bypass system includes.
def pl_bpf_preprocess(
        name,
        src,
        hdrs,
        syshdrs,
        tags = [],
        *kwargs):
    out_file = name

    # TODO(oazizi): -Isrc/stirling/bcc_bpf/system-headers should not be hard-coded, should come from syshdrs instead.
    cmd = "cpp -U linux -Dinclude=#include -I. -Isrc/stirling/bcc_bpf/system-headers $(location {}) -o $@".format(src)

    native.genrule(
        name = name + "_preprocess_rule",
        outs = [out_file],
        srcs = [src] + hdrs + [syshdrs],
        tags = tags,
        cmd = cmd,
    )
    return out_file
