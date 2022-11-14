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

# pl_toolchain_{pre,post}_features are used to extend the toolchain features of the default cc_toolchain_config.
# The ctx parameter is the rule ctx for the following rule signature:
#
#	cc_toolchain_config = rule(
#	    implementation = _impl,
#	    attrs = {
#	        "cpu": attr.string(mandatory = True),
#	        "compiler": attr.string(mandatory = True),
#	        "toolchain_identifier": attr.string(mandatory = True),
#	        "host_system_name": attr.string(mandatory = True),
#	        "target_system_name": attr.string(mandatory = True),
#	        "target_libc": attr.string(mandatory = True),
#	        "abi_version": attr.string(mandatory = True),
#	        "abi_libc_version": attr.string(mandatory = True),
#	        "cxx_builtin_include_directories": attr.string_list(),
#	        "tool_paths": attr.string_dict(),
#	        "compile_flags": attr.string_list(),
#	        "dbg_compile_flags": attr.string_list(),
#	        "opt_compile_flags": attr.string_list(),
#	        "cxx_flags": attr.string_list(),
#	        "link_flags": attr.string_list(),
#	        "link_libs": attr.string_list(),
#	        "opt_link_flags": attr.string_list(),
#	        "unfiltered_compile_flags": attr.string_list(),
#	        "coverage_compile_flags": attr.string_list(),
#	        "coverage_link_flags": attr.string_list(),
#	        "supports_start_end_lib": attr.bool(),
#	        "builtin_sysroot": attr.string(),
#	    },
#	    provides = [CcToolchainConfigInfo],
#	)
# Additionally, the attrs in PL_EXTRA_CC_CONFIG_ATTRS are available in the passed in ctx.
#

# pl_toolchain_pre_features are added to the command line before any of the default features.
def pl_toolchain_pre_features(ctx):
    return []

# pl_toolchain_post_features are added to the command line after all of the default features.
def pl_toolchain_post_features(ctx):
    return []

PL_EXTRA_CC_CONFIG_ATTRS = dict()
