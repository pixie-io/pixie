# Copyright 2018- The Pixie Authors.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# SPDX-License-Identifier: Apache-2.0

# This file contains rules for for our UI builds.

ui_shared_cmds_start = [
    'export BASE_PATH="$(pwd)"',
    "export PATH=/usr/local/bin:/opt/px_dev/tools/node/bin:$PATH",
    'export HOME="$(mktemp -d)"',  # This makes node-gyp happy.
    'export TMPPATH="$(mktemp -d)"',
]

ui_shared_cmds_finish = [
    "popd &> /dev/null",
    'rm -rf "$TMPPATH"',
]

def _pl_webpack_deps_impl(ctx):
    all_files = list(ctx.files.srcs)

    output_fname = "{}.tar.gz".format(ctx.attr.name)
    out = ctx.actions.declare_file(output_fname)

    cp_cmds = ["cp -aL --parents {} $TMPPATH".format(file.path) for file in all_files]

    cmd = ui_shared_cmds_start + cp_cmds + [
        'pushd "$TMPPATH/src/ui" &> /dev/null',
        "yarn install --immutable &> build.log",
        # Pick a deterministic mtime so that the output is not volatile.
        # This helps ensure that bazel can cache the ui builds as expected.
        'tar --mtime="2018-01-01 00:00:00 UTC" -czf "$BASE_PATH/{}" .'.format(out.path),
    ] + ui_shared_cmds_finish

    ctx.actions.run_shell(
        inputs = all_files,
        execution_requirements = {tag: "" for tag in ctx.attr.tags},
        outputs = [out],
        command = " && ".join(cmd),
        progress_message =
            "Generating webpack deps %s" % out.short_path,
    )

    return [
        DefaultInfo(
            files = depset([out]),
        ),
    ]

def _pl_webpack_library_impl(ctx):
    all_files = list(ctx.files.srcs)

    output_fname = "{}.tar.gz".format(ctx.attr.name)
    out = ctx.actions.declare_file(output_fname)

    env_cmds = []
    if ctx.attr.stamp:
        # This is some truly shady stuff. The stamping on genrules just makes this file
        # available, but does not apply it to the environment. We parse out the file
        # and apply it to the environment here. Hopefully,
        # no special characters/spaces/quotes in the results ...
        env_cmds = [
            '$(sed -E "s/^([A-Za-z_]+)\\s*(.*)/export \\1=\\2/g" "{}")'.format(ctx.info_file.path),
            '$(sed -E "s/^([A-Za-z_]+)\\s*(.*)/export \\1=\\2/g" "{}")'.format(ctx.version_file.path),
        ]
        all_files.append(ctx.info_file)
        all_files.append(ctx.version_file)

    cp_cmds = ["cp -aL --parents {} $TMPPATH".format(file.path) for file in all_files]

    cmd = env_cmds + ui_shared_cmds_start + cp_cmds + [
        'pushd "$TMPPATH/src/ui" &> /dev/null',
        'tar -xzf "$BASE_PATH/{}"'.format(ctx.file.deps.path),
        'mv -f "$BASE_PATH/{}" src/pages/credits/licenses.json'.format(ctx.file.licenses.path),
        "retval=0",
        "output=`yarn build_prod 2>&1` || retval=$?",
        '[ "$retval" -eq 0 ] || (echo $output; echo "Build Failed with Code: $retval"; exit $retval)',
        'cp dist/bundle.tar.gz "$BASE_PATH/{}"'.format(out.path),
    ] + ui_shared_cmds_finish

    ctx.actions.run_shell(
        inputs = all_files + ctx.files.deps + ctx.files.licenses,
        execution_requirements = {tag: "" for tag in ctx.attr.tags},
        outputs = [out],
        command = " && ".join(cmd),
        progress_message =
            "Generating webpack bundle %s" % out.short_path,
    )

    return [
        DefaultInfo(
            files = depset([out]),
        ),
    ]

def _pl_ui_test_impl(ctx):
    all_files = list(ctx.files.srcs)

    test_cmd = [
        "yarn test_ci",
    ]

    if ctx.configuration.coverage_enabled:
        test_cmd = [
            "yarn coverage_ci",
            "sed -i \"s|SF:src|SF:src/ui/src|g\" coverage/lcov.info",
            "cp coverage/lcov.info ${COVERAGE_OUTPUT_FILE}",
        ]

    cp_cmds = ["cp -aL --parents {} $TMPPATH".format(file.path) for file in all_files]

    cmd = ui_shared_cmds_start + cp_cmds + [
        'pushd "$TMPPATH/src/ui" &> /dev/null',
        "export JEST_JUNIT_OUTPUT_NAME=${XML_OUTPUT_FILE:-junit.xml}",
        'tar -xzf "$BASE_PATH/{}"'.format(ctx.file.deps.short_path),
    ] + test_cmd + ui_shared_cmds_finish

    script = " && ".join(cmd)

    ctx.actions.write(
        output = ctx.outputs.executable,
        content = script,
        is_executable = True,
    )

    runfiles = ctx.runfiles(files = ctx.files.srcs + ctx.files.deps)
    return [
        DefaultInfo(
            executable = ctx.outputs.executable,
            runfiles = runfiles,
        ),
        coverage_common.instrumented_files_info(
            ctx,
            source_attributes = ["srcs"],
            dependency_attributes = ["deps"],
            extensions = ["ts", "tsx", "js", "jsx"],
        ),
    ]

def _pl_deps_licenses_impl(ctx):
    all_files = list(ctx.files.srcs)

    output_fname = "{}.json".format(ctx.attr.name)
    out = ctx.actions.declare_file(output_fname)

    cp_cmds = ["cp -aL --parents {} $TMPPATH".format(file.path) for file in all_files]

    cmd = ui_shared_cmds_start + cp_cmds + [
        'pushd "$TMPPATH/src/ui" &> /dev/null',
        'export LIC_TMPPATH="$(mktemp -d)"',
        'tar -xzf "$BASE_PATH/{}"'.format(ctx.file.deps.path),
        "yarn license_check --excludePrivatePackages --production --json --out $LIC_TMPPATH/checker.json",
        'yarn pnpify node ./tools/licenses/yarn_license_extractor.js --input=$LIC_TMPPATH/checker.json --output="$BASE_PATH/{}"'.format(out.path),
    ] + ui_shared_cmds_finish

    ctx.actions.run_shell(
        inputs = all_files + ctx.files.deps,
        execution_requirements = {tag: "" for tag in ctx.attr.tags},
        outputs = [out],
        command = " && ".join(cmd),
        progress_message =
            "Generating licenses %s" % out.short_path,
    )
    return [
        DefaultInfo(
            files = depset([out]),
        ),
    ]

pl_webpack_deps = rule(
    implementation = _pl_webpack_deps_impl,
    attrs = dict({
        "srcs": attr.label_list(
            mandatory = True,
            allow_files = True,
        ),
    }),
)

pl_webpack_library = rule(
    implementation = _pl_webpack_library_impl,
    attrs = dict({
        "deps": attr.label(allow_single_file = True),
        "licenses": attr.label(allow_single_file = True),
        "srcs": attr.label_list(
            mandatory = True,
            allow_files = True,
        ),
        "stamp": attr.bool(mandatory = True),
    }),
)

_pl_ui_test = rule(
    implementation = _pl_ui_test_impl,
    attrs = dict({
        "deps": attr.label(allow_single_file = True),
        "srcs": attr.label_list(
            mandatory = True,
            allow_files = True,
        ),
        "_lcov_merger": attr.label(
            default = configuration_field(fragment = "coverage", name = "output_generator"),
            cfg = "exec",
        ),
    }),
    test = True,
)

def _pl_ui_test_macro(**kwargs):
    if "target_compatible_with" not in kwargs:
        kwargs["target_compatible_with"] = []
    kwargs["target_compatible_with"] = kwargs["target_compatible_with"] + select({
        "//bazel/cc_toolchains:libc_version_glibc_host": [],
        "//conditions:default": ["@platforms//:incompatible"],
    })
    _pl_ui_test(**kwargs)

pl_deps_licenses = rule(
    implementation = _pl_deps_licenses_impl,
    attrs = dict({
        "deps": attr.label(allow_single_file = True),
        "srcs": attr.label_list(
            mandatory = True,
            allow_files = True,
        ),
    }),
)

pl_ui_test = _pl_ui_test_macro
