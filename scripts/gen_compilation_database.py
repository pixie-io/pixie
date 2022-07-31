#!/usr/bin/env python

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

# Borrowed from: https://github.com/envoyproxy/envoy/commit/24e2e0e735347a0440fe456ddc328053ba095daa

import argparse
import os
import json
import subprocess


def generateCompilationDatabase(args):
    if args.run_bazel_build:
        subprocess.check_call(["bazel", "build"] + [args.bazel_target])

    gen_compilation_database_sh = os.path.join(
        os.path.realpath(os.path.dirname(__file__)), "gen_compilation_database.sh")
    subprocess.check_call([gen_compilation_database_sh] + [args.bazel_target])


def isHeader(filename):
    for ext in (".h", ".hh", ".hpp", ".hxx"):
        if filename.endswith(ext):
            return True
    return False


def isCompileTarget(target, args):
    filename = target["file"]
    if not args.include_headers and isHeader(filename):
        return False

    if not args.include_genfiles:
        if filename.startswith("bazel-out/"):
            return False

    if not args.include_external:
        if filename.startswith("external/"):
            return False

    if not args.include_external:
        if filename.startswith("third_party/"):
            return False

    # TODO(oazizi): Remove this after you fix includes. Disable
    # compilation database for bpftrace files.
    if filename.endswith(".bt"):
        return False

    return True


def modifyCompileCommand(target, args):
    _, options = target["command"].split(" ", 1)

    # Workaround for bazel added C++11 options, those doesn't affect build itself but
    # clang-tidy will misinterpret them.
    options = options.replace("-std=c++0x ", "")
    options = options.replace("-std=c++11 ", "")

    if args.vscode:
        # Visual Studio Code doesn't seem to like "-iquote". Replace it with
        # old-style "-I".
        options = options.replace("-iquote ", "-I ")

    options = options.replace(
        "__BAZEL_XCODE_SDKROOT__",
        "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/")
    if isHeader(target["file"]):
        options += " -Wno-pragma-once-outside-header -Wno-unused-const-variable"
        options += " -Wno-unused-function"

    target["command"] = " ".join(["clang++", options])
    return target


def fixCompilationDatabase(args):
    with open("compile_commands.json", "r") as db_file:
        db = json.load(db_file)

    db = [modifyCompileCommand(target, args)
          for target in db if isCompileTarget(target, args)]

    db = make_paths_stable(db)

    # Remove to avoid writing into symlink.
    os.remove("compile_commands.json")
    with open("compile_commands.json", "w") as db_file:
        json.dump(db, db_file, indent=2)


def get_bazel_workspace_root():
    res = subprocess.run(["bazel", "info", "workspace"], capture_output=True)
    res.check_returncode()
    return res.stdout.decode('utf-8').strip()


def get_bazel_output_base():
    res = subprocess.run(["bazel", "info", "output_base"], capture_output=True)
    res.check_returncode()
    return res.stdout.decode('utf-8').strip()


def make_paths_stable(db):
    workspace_root = get_bazel_workspace_root()
    bazel_output_base = get_bazel_output_base()
    external_path = os.path.join(bazel_output_base, "external")
    external_symlink = os.path.join(workspace_root, "external")
    # Make a symlink to external at the top of the repo, so that we don't have to modify compile commands.
    subprocess.run(["ln", "-sf", external_path, external_symlink]).check_returncode()

    for target in db:
        target["directory"] = workspace_root
    return db


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description='Generate JSON compilation database')
    parser.add_argument('--run_bazel_build', action='store_true')
    parser.add_argument('--include_external', action='store_true')
    parser.add_argument('--include_genfiles', action='store_true')
    parser.add_argument('--include_headers', action='store_true')
    parser.add_argument('--vscode', action='store_true')
    parser.add_argument(
        '--bazel_target', default="//src/...")
    args = parser.parse_args()
    generateCompilationDatabase(args)
    fixCompilationDatabase(args)
