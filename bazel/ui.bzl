# This file contains rules for for our UI builds.
# It's a bit hacky, but better than what we had before. This is just a placeholder until
# https://github.com/bazelbuild/rules_nodejs is ready. In the current state bazel
# caching is broken with rules_nodejs.

ui_shared_cmds_start = [
    "export BASE_PATH=$PWD",
    "export PATH=/usr/local/bin:/opt/node/bin:$PATH",
    "export HOME=$(mktemp -d)",  # This makes node-gyp happy.
    "export TMPPATH=$(mktemp -d)",
    # This is some truly shady stuff. The stamping on genrules just makes this file
    # available, but does not apply it to the environment. We parse out the file
    # and apply it to the environment here. Hopefully,
    # no special characters/spaces/quotes in the results ...
    'if [ -f "bazel-out/stable-status.txt" ]; then\n' +
    "$(sed -E \"s/^([A-Za-z_]+)\\s*(.*)/export \\1=\\2/g\" bazel-out/stable-status.txt)\n" +
    "fi",
    'if [ -f "bazel-out/volatile-status.txt" ]; then\n' +
    "$(sed -E \"s/^([A-Za-z_]+)\\s*(.*)/export \\1=\\2/g\" bazel-out/volatile-status.txt)\n" +
    "fi",
    "cp -aL ${BASE_PATH}/* ${TMPPATH}",
    "pushd ${TMPPATH}/${UILIB_PATH} &> /dev/null",
]

ui_shared_cmds_finish = [
    "popd &> /dev/null",
    "rm -rf ${TMPPATH}",
]

def _webpack_deps_impl(ctx):
    all_files = list(ctx.files.srcs)

    cmd = ui_shared_cmds_start + [
        "export OUTPUT_PATH=" + ctx.outputs.out.path,
        "yarn install --prefer_offline &> build.log",
        "tar -czf ${BASE_PATH}/${OUTPUT_PATH} node_modules .",
    ] + ui_shared_cmds_finish

    ctx.actions.run_shell(
        inputs = all_files,
        outputs = [ctx.outputs.out],
        command = " && ".join(cmd),
        env = {
            "BASE_PATH": "$$PWD",
            "UILIB_PATH": ctx.attr.uilib_base,
        },
        progress_message =
            "Generating webpack deps %s" % ctx.outputs.out.short_path,
    )

def _webpack_binary_impl(ctx):
    all_files = list(ctx.files.srcs)

    if ctx.attr.stamp:
        all_files.append(ctx.info_file)
        all_files.append(ctx.version_file)

    cmd = ui_shared_cmds_start + [
        "export OUTPUT_PATH=" + ctx.outputs.out.path,
        "tar -zxf ${BASE_PATH}/" + ctx.file.deps.path,
        "mv -f ${BASE_PATH}/" + ctx.file.licenses.path + " src/pages/credits/licenses.json",
        "yarn build_prod",
        "cp dist/bundle.tar.gz ${BASE_PATH}/${OUTPUT_PATH}",
    ] + ui_shared_cmds_finish

    ctx.actions.run_shell(
        inputs = all_files + ctx.files.deps + ctx.files.licenses,
        outputs = [ctx.outputs.out],
        command = " && ".join(cmd),
        env = {
            "BASE_PATH": "$$PWD",
            "UILIB_PATH": ctx.attr.uilib_base,
        },
        progress_message =
            "Generating webpack bundle %s" % ctx.outputs.out.short_path,
    )

def _pl_ui_test_impl(ctx):
    all_files = list(ctx.files.srcs)

    cmd = [
        "export BASE_PATH=$(pwd)",
        "export UILIB_PATH=" + ctx.attr.uilib_base,
    ] + ui_shared_cmds_start + [
        "export OUTPUT_PATH_LCOV=${TEST_UNDECLARED_OUTPUTS_DIR}/lcov.info",
        "export OUTPUT_PATH_JUNIT=${TEST_UNDECLARED_OUTPUTS_DIR}/junit.xml",
        "printenv",
        "tar -zxf ${BASE_PATH}/" + ctx.file.deps.short_path,
        "yarn coverage &> build.log",
        "cp coverage/lcov.info ${OUTPUT_PATH_LCOV}",
        "cp junit.xml ${OUTPUT_PATH_JUNIT}",
    ] + ui_shared_cmds_finish

    script = " && ".join(cmd)

    ctx.actions.write(
        output = ctx.outputs.executable,
        content = script,
    )

    runfiles = ctx.runfiles(files = [ctx.outputs.executable] + all_files + ctx.files.deps)
    return [DefaultInfo(runfiles = runfiles)]

def _pl_storybook_binary_impl(ctx):
    all_files = list(ctx.files.srcs)

    if ctx.attr.stamp:
        all_files.append(ctx.info_file)
        all_files.append(ctx.version_file)

    cmd = ui_shared_cmds_start + [
        "export OUTPUT_PATH=" + ctx.outputs.out.path,
        "tar -zxf ${BASE_PATH}/" + ctx.file.deps.path,
        "yarn workspace pixie-components storybook_static",
        # Write the outputs to the correct location so Bazel can find them.
        "pushd packages/pixie-components",
        "tar -czf ${BASE_PATH}/${OUTPUT_PATH} storybook_static",
        "popd",
    ] + ui_shared_cmds_finish

    ctx.actions.run_shell(
        inputs = all_files + ctx.files.deps,
        outputs = [ctx.outputs.out],
        command = " && ".join(cmd),
        env = {
            "BASE_PATH": "$$PWD",
            "UILIB_PATH": ctx.attr.uilib_base,
        },
        progress_message =
            "Generating storybook bundle %s" % ctx.outputs.out.short_path,
    )

def _deps_licenses_impl(ctx):
    all_files = list(ctx.files.srcs)

    cmd = ui_shared_cmds_start + [
        "export OUTPUT_PATH=" + ctx.outputs.out.path,
        "tar -zxf ${BASE_PATH}/" + ctx.file.deps.path,
        "yarn license-checker --excludePrivatePackages --production --json --out ${TMPPATH}/checker.json",
        "python3 tools/licenses/npm_license_extractor.py " +
        "--input=${TMPPATH}/checker.json --output=${BASE_PATH}/${OUTPUT_PATH}",
    ] + ui_shared_cmds_finish

    ctx.actions.run_shell(
        inputs = all_files + ctx.files.deps,
        outputs = [ctx.outputs.out],
        command = " && ".join(cmd),
        env = {
            "BASE_PATH": "$$PWD",
            "UILIB_PATH": ctx.attr.uilib_base,
        },
        progress_message =
            "Generating licenses %s" % ctx.outputs.out.short_path,
    )

pl_webpack_deps = rule(
    implementation = _webpack_deps_impl,
    attrs = dict({
        "srcs": attr.label_list(
            mandatory = True,
            allow_files = True,
        ),
        "uilib_base": attr.string(
            doc = "This is a slight hack that requires the basepath to package.json relative to TOT to be specified",
        ),
    }),
    outputs = {
        "out": "%{name}.tar.gz",
    },
)

pl_webpack_binary = rule(
    implementation = _webpack_binary_impl,
    attrs = dict({
        "srcs": attr.label_list(
            mandatory = True,
            allow_files = True,
        ),
        "deps": attr.label(allow_single_file = True),
        "licenses": attr.label(allow_single_file = True),
        "stamp": attr.bool(mandatory = True),
        "uilib_base": attr.string(
            doc = "This is a slight hack that requires the basepath to package.json relative to TOT to be specified",
        ),
    }),
    outputs = {
        "out": "%{name}.tar.gz",
    },
)

pl_ui_test = rule(
    implementation = _pl_ui_test_impl,
    attrs = dict({
        "srcs": attr.label_list(
            mandatory = True,
            allow_files = True,
        ),
        "deps": attr.label(allow_single_file = True),
        "uilib_base": attr.string(
            doc = "This is a slight hack that requires the basepath to package.json relative to TOT to be specified",
        ),
    }),
    test = True,
)

pl_storybook_binary = rule(
    implementation = _pl_storybook_binary_impl,
    attrs = dict({
        "srcs": attr.label_list(
            mandatory = True,
            allow_files = True,
        ),
        "deps": attr.label(allow_single_file = True),
        "stamp": attr.bool(mandatory = True),
        "uilib_base": attr.string(
            doc = "This is a slight hack that requires the basepath to package.json relative to TOT to be specified",
        ),
    }),
    outputs = {
        "out": "%{name}.tar.gz",
    },
)

pl_deps_licenses = rule(
    implementation = _deps_licenses_impl,
    attrs = dict({
        "srcs": attr.label_list(
            mandatory = True,
            allow_files = True,
        ),
        "deps": attr.label(allow_single_file = True),
        "uilib_base": attr.string(
            doc = "This is a slight hack that requires the basepath to package.json relative to TOT to be specified",
        ),
    }),
    outputs = {
        "out": "%{name}.json",
    },
)
