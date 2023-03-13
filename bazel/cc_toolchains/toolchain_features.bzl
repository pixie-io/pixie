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
load("@bazel_tools//tools/build_defs/cc:action_names.bzl", "ACTION_NAMES")
load(
    "@bazel_tools//tools/cpp:cc_toolchain_config_lib.bzl",
    "action_config",
    "feature",
    "flag_group",
    "flag_set",
    "tool",
)

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
    features = []
    features += _custom_c_runtime_path_pre(ctx)
    return features

# pl_toolchain_post_features are added to the command line after all of the default features.
def pl_toolchain_post_features(ctx):
    features = []
    features += _libstdcpp(ctx)
    if ctx.attr.compiler == "clang":
        features += _clang_features(ctx)

    features += _external_dep(ctx)

    if len(ctx.attr.unfiltered_link_flags) > 0:
        features += _unfiltered_link_flags(ctx)
    features += _custom_c_runtime_path_post(ctx)
    return features

PL_EXTRA_CC_CONFIG_ATTRS = dict(
    libclang_rt_path = attr.string(),
    enable_sanitizers = attr.bool(),
    custom_c_runtime_paths = attr.string_dict(
        default = {
            "gcc": "",
            "sysroot": "",
        },
    ),
    unfiltered_link_flags = attr.string_list(),
    libcxx_path = attr.string(),
)

all_compile_actions = [
    ACTION_NAMES.c_compile,
    ACTION_NAMES.cpp_compile,
    ACTION_NAMES.linkstamp_compile,
    ACTION_NAMES.assemble,
    ACTION_NAMES.preprocess_assemble,
    ACTION_NAMES.cpp_header_parsing,
    ACTION_NAMES.cpp_module_compile,
    ACTION_NAMES.cpp_module_codegen,
    ACTION_NAMES.clif_match,
    ACTION_NAMES.lto_backend,
]

all_cpp_compile_actions = [
    ACTION_NAMES.cpp_compile,
    ACTION_NAMES.linkstamp_compile,
    ACTION_NAMES.cpp_header_parsing,
    ACTION_NAMES.cpp_module_compile,
    ACTION_NAMES.cpp_module_codegen,
    ACTION_NAMES.clif_match,
]

all_link_actions = [
    ACTION_NAMES.cpp_link_executable,
    ACTION_NAMES.cpp_link_dynamic_library,
    ACTION_NAMES.cpp_link_nodeps_dynamic_library,
]
lto_index_actions = [
    ACTION_NAMES.lto_index_for_executable,
    ACTION_NAMES.lto_index_for_dynamic_library,
    ACTION_NAMES.lto_index_for_nodeps_dynamic_library,
]

def _libstdcpp(ctx):
    return [
        feature(
            name = "libstdc++",
            enabled = True,
            flag_sets = [
                flag_set(
                    actions = all_link_actions + lto_index_actions,
                    flag_groups = [
                        flag_group(
                            flags = [
                                "-l:libstdc++.a",
                            ],
                        ),
                    ],
                ),
            ],
            provides = ["stdlib++"],
        ),
    ]

def _libcpp(ctx):
    return [
        feature(
            name = "libc++",
            flag_sets = [
                flag_set(
                    actions = all_cpp_compile_actions + [ACTION_NAMES.lto_backend],
                    flag_groups = [
                        flag_group(
                            flags = [
                                "-stdlib=libc++",
                                "-isystem{libcxx_path}/include/c++/v1".format(libcxx_path = ctx.attr.libcxx_path),
                            ],
                        ),
                    ],
                ),
                flag_set(
                    actions = all_link_actions + lto_index_actions,
                    flag_groups = [
                        flag_group(
                            flags = [
                                "-L{libcxx_path}/lib".format(libcxx_path = ctx.attr.libcxx_path),
                                "-l:libc++.a",
                                "-l:libc++abi.a",
                            ],
                        ),
                    ],
                ),
            ],
            provides = ["stdlib++"],
        ),
    ]

def _clang_features(ctx):
    features = []
    features += _libcpp(ctx)
    if ctx.attr.enable_sanitizers:
        if ctx.attr.libclang_rt_path != "":
            features += _asan(ctx)
        features += _msan(ctx)
        features += _tsan(ctx)
    return features

def _asan(ctx):
    return [
        feature(
            name = "asan",
            flag_sets = [
                flag_set(
                    actions = all_compile_actions,
                    flag_groups = [
                        flag_group(
                            flags = [
                                "-DPL_CONFIG_ASAN",
                                "-D__SANITIZE_ADDRESS__",
                                "-fsanitize=address,undefined",
                                "-fno-sanitize=vptr",
                                "-fsanitize-recover=all",
                                "-DADDRESS_SANITIZER=1",
                            ],
                        ),
                    ],
                ),
                flag_set(
                    actions = all_link_actions + lto_index_actions,
                    flag_groups = [
                        flag_group(
                            flags = [
                                "-fsanitize=address,undefined",
                                "-fno-sanitize=vptr",
                                "-ldl",
                                "-L" + ctx.attr.libclang_rt_path,
                                "-l:libclang_rt.ubsan_standalone-x86_64.a",
                                "-l:libclang_rt.ubsan_standalone_cxx-x86_64.a",
                                "-l:libclang_rt.builtins-x86_64.a",
                            ],
                        ),
                    ],
                ),
            ],
            provides = ["sanitizer"],
            implies = ["libc++"],
        ),
    ]

def _msan(ctx):
    return [
        feature(
            name = "msan",
            flag_sets = [
                flag_set(
                    actions = all_compile_actions,
                    flag_groups = [
                        flag_group(
                            flags = [
                                "-fsanitize=memory",
                                "-fsanitize-memory-track-origins=2",
                                "-DMEMORY_SANITIZER=1",
                            ],
                        ),
                    ],
                ),
                flag_set(
                    actions = all_link_actions + lto_index_actions,
                    flag_groups = [
                        flag_group(
                            flags = [
                                "-fsanitize=memory",
                            ],
                        ),
                    ],
                ),
            ],
            provides = ["sanitizer"],
            implies = ["libc++"],
        ),
    ]

def _tsan(ctx):
    return [
        feature(
            name = "tsan",
            flag_sets = [
                flag_set(
                    actions = all_compile_actions,
                    flag_groups = [
                        flag_group(
                            flags = [
                                "-fsanitize=thread",
                                "-fsanitize-recover=all",
                                "-DTHREAD_SANITIZER=1",
                            ],
                        ),
                    ],
                ),
                flag_set(
                    actions = all_link_actions + lto_index_actions,
                    flag_groups = [
                        flag_group(
                            flags = [
                                "-fsanitize=thread",
                            ],
                        ),
                    ],
                ),
            ],
            provides = ["sanitizer"],
            implies = ["libc++"],
        ),
    ]

def _unfiltered_link_flags(ctx):
    return [
        feature(
            name = "unfiltered_link_flags",
            enabled = True,
            flag_sets = [
                flag_set(
                    actions = all_link_actions + lto_index_actions,
                    flag_groups = [
                        flag_group(
                            flags = ctx.attr.unfiltered_link_flags,
                        ),
                    ],
                ),
            ],
        ),
    ]

def _custom_c_runtime_path_pre(ctx):
    enable = ctx.attr.custom_c_runtime_paths["sysroot"] != "" and ctx.attr.custom_c_runtime_paths["gcc"] != ""
    return [
        feature(
            name = "c_runtime_flags_pre",
            enabled = True,
            flag_sets = [
                flag_set(
                    actions = [
                        ACTION_NAMES.cpp_link_executable,
                        ACTION_NAMES.lto_index_for_executable,
                    ],
                    flag_groups = [
                        flag_group(
                            flags = [
                                ctx.attr.custom_c_runtime_paths["sysroot"] + "/Scrt1.o",
                            ],
                        ),
                    ] if enable else [],
                ),
                flag_set(
                    actions = all_link_actions + lto_index_actions,
                    flag_groups = [
                        flag_group(
                            flags = [
                                ctx.attr.custom_c_runtime_paths["sysroot"] + "/crti.o",
                                ctx.attr.custom_c_runtime_paths["gcc"] + "/crtbeginS.o",
                            ],
                        ),
                    ] if enable else [],
                ),
            ],
        ),
    ]

def _custom_c_runtime_path_post(ctx):
    enable = ctx.attr.custom_c_runtime_paths["sysroot"] != "" and ctx.attr.custom_c_runtime_paths["gcc"] != ""
    return [
        feature(
            name = "c_runtime_flags_post",
            enabled = True,
            flag_sets = [
                flag_set(
                    actions = all_link_actions + lto_index_actions,
                    flag_groups = [
                        flag_group(
                            flags = [
                                ctx.attr.custom_c_runtime_paths["gcc"] + "/crtendS.o",
                                ctx.attr.custom_c_runtime_paths["sysroot"] + "/crtn.o",
                            ],
                        ),
                    ] if enable else [],
                ),
            ],
        ),
    ]

def _external_dep(ctx):
    clang_disable_warnings = ["-Wno-everything"]
    gcc_disable_warnings = ["-w"]
    return [
        feature(
            name = "external_dep",
            enabled = True,
            flag_sets = [
                flag_set(
                    actions = all_compile_actions,
                    flag_groups = [
                        flag_group(
                            flags = clang_disable_warnings if ctx.attr.compiler == "clang" else gcc_disable_warnings,
                        ),
                    ],
                ),
            ],
        ),
    ]

def _objcopy_action(ctx):
    return action_config(
        action_name = "objcopy",
        enabled = True,
        tools = [
            tool(path = ctx.attr.tool_paths["objcopy"]),
        ],
    )

def _cpp_action(ctx):
    return action_config(
        action_name = "c-preprocess",
        enabled = True,
        tools = [
            tool(path = ctx.attr.tool_paths["cpp"]),
        ],
    )

def pl_action_configs(ctx):
    return [
        _objcopy_action(ctx),
        _cpp_action(ctx),
    ]
