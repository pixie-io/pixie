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

def _pl_webpack_deps_impl(ctx):
    all_files = list(ctx.files.srcs)

    cmd = ui_shared_cmds_start + [
        "export OUTPUT_PATH=" + ctx.outputs.out.path,
        "yarn install --immutable &> build.log",
        "tar -czf ${BASE_PATH}/${OUTPUT_PATH} .",
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

def _pl_webpack_library_impl(ctx):
    all_files = list(ctx.files.srcs)

    if ctx.attr.stamp:
        all_files.append(ctx.info_file)
        all_files.append(ctx.version_file)

    cmd = ui_shared_cmds_start + [
        "export OUTPUT_PATH=" + ctx.outputs.out.path,
        "tar -zxf ${BASE_PATH}/" + ctx.file.deps.path,
        "[ ! -d src/configurables/private ] || mv ${BASE_PATH}/" + ctx.file.licenses.path + " src/configurables/private/licenses.json",
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
    test_cmd = [
        "yarn test_ci",
    ]

    if ctx.configuration.coverage_enabled:
        test_cmd = [
            "yarn coverage_ci",
            "cp coverage/lcov.info ${COVERAGE_OUTPUT_FILE}",
        ]

    cmd = [
        "export BASE_PATH=$(pwd)",
        "export UILIB_PATH=" + ctx.attr.uilib_base,
        "export JEST_JUNIT_OUTPUT_NAME=${XML_OUTPUT_FILE:-junit.xml}",
    ] + ui_shared_cmds_start + [
        "tar -zxf ${BASE_PATH}/" + ctx.file.deps.short_path,
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

    cmd = ui_shared_cmds_start + [
        "export OUTPUT_PATH=" + ctx.outputs.out.path,
        "tar -zxf ${BASE_PATH}/" + ctx.file.deps.path,
        "yarn license_check --excludePrivatePackages --production --json --out ${TMPPATH}/checker.json",
        "yarn pnpify node ./tools/licenses/yarn_license_extractor.js " +
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
    implementation = _pl_webpack_deps_impl,
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
        "deps": attr.label(allow_single_file = True),
        "srcs": attr.label_list(
            mandatory = True,
            allow_files = True,
        ),
        "uilib_base": attr.string(
            doc = "This is a slight hack that requires the basepath to package.json relative to TOT to be specified",
        ),
        # Workaround for bazelbuild/bazel#6293. See comment in lcov_merger.sh.
        # This dummy binary is a shell script that just exits with no error.
        "_lcov_merger": attr.label(
            executable = True,
            default = Label("@io_bazel_rules_go//go/tools/builders:lcov_merger"),
            cfg = "target",
        ),
    }),
    test = True,
)

pl_deps_licenses = rule(
    implementation = _pl_deps_licenses_impl,
    attrs = dict({
        "deps": attr.label(allow_single_file = True),
        "srcs": attr.label_list(
            mandatory = True,
            allow_files = True,
        ),
        "uilib_base": attr.string(
            doc = "This is a slight hack that requires the basepath to package.json relative to TOT to be specified",
        ),
    }),
    outputs = {
        "out": "%{name}.json",
    },
)
