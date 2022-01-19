# Copyright 2017 The Bazel Authors. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# SPDX-License-Identifier: Apache-2.0
#
# This file is unmodified from it's existing version at:
# https://raw.githubusercontent.com/bazelbuild/bazel-toolchains/v5.0.0/rules/gcs.bzl

"""Repository rule for pulling a file from GCS bucket.

The rule uses gsutil tool installed in the system to download a file from a GCS bucket,
and make it available for other rules to use (e.g. container_image rule).
To install gsutil, please refer to:
  https://cloud.google.com/storage/docs/gsutil
You need to have read access to the GCS bucket.
"""

_GCS_FILE_BUILD = """
package(default_visibility = ["//visibility:public"])
filegroup(
    name = "file",
    srcs = ["{}"],
)
"""

def _gcs_file_impl(ctx):
    """Implementation of the gcs_file rule."""
    repo_root = ctx.path(".")
    forbidden_files = [
        repo_root,
        ctx.path("WORKSPACE"),
        ctx.path("BUILD"),
        ctx.path("BUILD.bazel"),
        ctx.path("file/BUILD"),
        ctx.path("file/BUILD.bazel"),
    ]
    downloaded_file_path = ctx.attr.downloaded_file_path or ctx.attr.file
    download_path = ctx.path("file/" + downloaded_file_path)
    if download_path in forbidden_files or not str(download_path).startswith(str(repo_root)):
        fail("'%s' cannot be used as downloaded_file_path in gcs_file" % ctx.attr.downloaded_file_path)

    # Add a top-level BUILD file to export all the downloaded files.
    ctx.file("file/BUILD", _GCS_FILE_BUILD.format(downloaded_file_path))

    # Create a bash script from a template.
    ctx.template(
        "gsutil_cp_and_validate.sh",
        Label("@bazel_toolchains//rules:gsutil_cp_and_validate.sh.tpl"),
        {
            "%{BUCKET}": ctx.attr.bucket,
            "%{DOWNLOAD_PATH}": str(download_path),
            "%{FILE}": ctx.attr.file,
            "%{SHA256}": ctx.attr.sha256,
        },
    )

    gsutil_cp_and_validate_result = ctx.execute(["bash", "gsutil_cp_and_validate.sh"])
    if gsutil_cp_and_validate_result.return_code == 255:
        fail("SHA256 of file {} from bucket {} does not match given SHA256: {} {}".format(
            ctx.attr.file,
            ctx.attr.bucket,
            gsutil_cp_and_validate_result.stdout,
            gsutil_cp_and_validate_result.stderr,
        ))
    elif gsutil_cp_and_validate_result.return_code != 0:
        fail("gsutil cp command failed: %s" % (gsutil_cp_and_validate_result.stderr))

    rm_result = ctx.execute(["rm", "gsutil_cp_and_validate.sh"])
    if rm_result.return_code:
        fail("Failed to remove temporary file: %s" % rm_result.stderr)

gcs_file = repository_rule(
    attrs = {
        "bucket": attr.string(
            mandatory = True,
            doc = "The GCS bucket which contains the file.",
        ),
        "downloaded_file_path": attr.string(
            doc = "Path assigned to the file downloaded.",
        ),
        "file": attr.string(
            mandatory = True,
            doc = "The file which we are downloading.",
        ),
        "sha256": attr.string(
            mandatory = True,
            doc = "The expected SHA-256 of the file downloaded.",
        ),
    },
    implementation = _gcs_file_impl,
)
