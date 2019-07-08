load("//bazel:pl_bpf_preprocess.bzl", "pl_bpf_preprocess")

def pl_cc_resource(
        name,
        srcs,
        tags = [],
        **kwargs):
    out_files = []
    for src in srcs:
        out_file = src + ".copied"
        native.genrule(
            name = src + "_cp_genrule",
            outs = [out_file],
            srcs = [src],
            tags = tags,
            cmd = "cat $(location {0}) > $@".format(src),
            **kwargs
        )
        out_files.append(out_file)
    pl_cc_resource_impl(name, out_files, tags, **kwargs)

def pl_bpf_cc_resource(
        name,
        hdrs,
        srcs,
        tags = [],
        **kwargs):
    out_files = []
    for i in range(len(srcs)):
        out_file = pl_bpf_preprocess(name, hdrs[i], srcs[i], tags)
        out_files.append(out_file)
    pl_cc_resource_impl(name, out_files, tags = [], **kwargs)

def pl_cc_resource_impl(
        name,
        srcs,
        tags = [],
        **kwargs):
    object_files = []
    tags = ["linux_only"] + tags

    # Loop through srcs and run genrule to generate a .o file.
    for src in srcs:
        object_file = src + ".o"
        native.genrule(
            name = src + "_genrule",
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
        object_files.append(object_file)

    # Create a cc_library with the .o files.
    native.cc_library(name = name, srcs = object_files, tags = tags, linkstatic = 1, **kwargs)

def pl_exp_cc_resource(**kwargs):
    tags = kwargs.get("tags", [])
    kwargs["tags"] = tags + ["manual"]
    pl_cc_resource(**kwargs)
