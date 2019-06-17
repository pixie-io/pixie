def pl_bpf_preprocess(
        name,
        hdrs,
        src,
        tags = [],
        *kwargs):
    out_file = src + ".preprocessed"
    cmd = "$(location //src/stirling/utils:bpf_header) " + \
          "--input_file=$(location {0}) ".format(src)
    for h in hdrs:
        cmd += "--header_files=$(location {0}) ".format(h)
    cmd += "--output_file $@"
    native.genrule(
        name = src + "preprocess_genrule",
        outs = [out_file],
        srcs = hdrs + [src],
        tags = tags,
        cmd = cmd,
        tools = ["//src/stirling/utils:bpf_header"],
    )
    return out_file
