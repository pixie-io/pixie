load("@rules_cc//cc:defs.bzl", "cc_library")
load("//bazel:pl_bpf_preprocess.bzl", "pl_bpf_preprocess")

def pl_cc_resource(
        name,
        src,
        tags = [],
        **kwargs):
    # The name chosen here will determine the symbol in the object file.
    out_file = src + "_src"
    native.genrule(
        name = name + src + "_cp_genrule",
        outs = [out_file],
        srcs = [src],
        tags = tags,
        cmd = "cat $(location {0}) > $@".format(src),
        **kwargs
    )
    pl_cc_resource_impl(name, out_file, tags, **kwargs)

def pl_bpf_cc_resource(
        name,
        src,
        hdrs,
        syshdrs,
        tags = [],
        **kwargs):
    # The name chosen here will determine the name of out_file, which will, in turn,
    # determine the symbol in the object file.
    out_file = pl_bpf_preprocess(name + "_bpf_src", src, hdrs, syshdrs, tags)
    pl_cc_resource_impl(name, out_file, tags = [], **kwargs)

def pl_cc_resource_impl(
        name,
        src,
        tags = [],
        **kwargs):
    object_files = []
    tags = ["linux_only"] + tags

    object_file = src + ".o"
    native.genrule(
        name = name + src + "_genrule",
        outs = [object_file],
        srcs = [src],
        tags = tags,
        toolchains = ["@bazel_tools//tools/cpp:current_cc_toolchain"],
        # This is because the preprocessed files are now in Bazel's rule dir and $(location)
        # will return the path of the source file, not the preprocessed file. So we cd into
        # $(RULEDIR) and use the original file name to find the files.
        cmd = " cd $(RULEDIR) && $(OBJCOPY) --input binary" +
              " --output elf64-x86-64" +
              " --binary-architecture i386:x86-64" +
              " {0} {1};".format(src, object_file),
        **kwargs
    )

    # Create a cc_library with the .o file.
    cc_library(name = name, srcs = [object_file], tags = tags, linkstatic = 1, **kwargs)

def pl_exp_cc_resource(**kwargs):
    tags = kwargs.get("tags", [])
    kwargs["tags"] = tags + ["manual"]
    pl_cc_resource(**kwargs)
