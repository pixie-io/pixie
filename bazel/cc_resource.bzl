def pl_cc_resource(
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
            cmd = " $(OBJCOPY) --input binary" +
                  " --output elf64-x86-64" +
                  " --binary-architecture i386:x86-64" +
                  " $(location {0}) $(location {1});".format(src, object_file),
            **kwargs
        )
        object_files.append(object_file)

    # Create a cc_library with the .o files.
    native.cc_library(name = name, srcs = object_files, tags = tags, linkstatic = 1, **kwargs)
