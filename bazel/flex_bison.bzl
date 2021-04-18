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

# Taken from: https://github.com/kythe/kythe/blob/master/tools/build_rules/lexyacc.bzl

def genflex(name, src, out, includes = [], **kwargs):
    """Generate a C++ lexer from a lex file using Flex.
    Args:
      name: The name of the rule.
      src: The .lex source file.
      out: The generated source file.
      includes: A list of headers included by the .lex file.
    """
    cmd = "flex -o $(@D)/%s $(location %s)" % (out, src)
    native.genrule(
        name = name,
        outs = [out],
        srcs = [src] + includes,
        cmd = cmd,
        **kwargs
    )

def genbison(name, src, header_out, source_out, extra_outs = [], **kwargs):
    """Generate a C++ parser from a Yacc file using Bison.
    Args:
      name: The name of the rule.
      src: The input grammar file.
      header_out: The generated header file.
      source_out: The generated source file.
      extra_outs: Additional generated outputs.
    """
    arg_adjust = "$$(bison --version | grep -qE '^bison .* 3\\..*' && echo -Wno-deprecated)"
    cmd = "bison %s -o $(@D)/%s $(location %s)" % (arg_adjust, source_out, src)
    native.genrule(
        name = name,
        outs = [source_out, header_out] + extra_outs,
        srcs = [src],
        cmd = cmd,
        **kwargs
    )
