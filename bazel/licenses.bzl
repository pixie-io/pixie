def _fetch_licenses_impl(ctx):
    args = ctx.actions.args()
    args.add("--github_token", ctx.file.oauth_token)
    args.add("--modules", ctx.file.src)

    if ctx.attr.use_pkg_dev_go:
        args.add("--try_pkg_dev_go")

    if ctx.attr.disallow_missing:
        args.add("--fatal_if_missing")

    args.add("--json_manual_input", ctx.file.manual_licenses)
    args.add("--json_output", ctx.outputs.out_found)
    args.add("--json_missing_output", ctx.outputs.out_missing)

    ctx.actions.run(
        executable = ctx.file.fetch_tool,
        inputs = [ctx.file.src, ctx.file.oauth_token, ctx.file.manual_licenses],
        outputs = [ctx.outputs.out_found, ctx.outputs.out_missing],
        arguments = [args],
        progress_message =
            "Fetching licenses %s" % ctx.outputs.out_found,
    )

fetch_licenses = rule(
    implementation = _fetch_licenses_impl,
    attrs = dict({
        "disallow_missing": attr.bool(),
        "fetch_tool": attr.label(mandatory = True, allow_single_file = True),
        "manual_licenses": attr.label(mandatory = True, allow_single_file = True),
        "oauth_token": attr.label(mandatory = True, allow_single_file = True),
        "out_found": attr.output(mandatory = True),
        "out_missing": attr.output(),
        "src": attr.label(mandatory = True, allow_single_file = True),
        "use_pkg_dev_go": attr.bool(),
    }),
)
