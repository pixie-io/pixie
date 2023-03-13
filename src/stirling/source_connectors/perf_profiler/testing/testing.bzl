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

agent_libs = [
    "//src/stirling/source_connectors/perf_profiler/java/agent:agent",
]

px_jattach = "//src/stirling/source_connectors/perf_profiler/java/px_jattach:px_jattach"

agent_libs_arg = ",".join(["$(location %s)" % lib for lib in agent_libs])
px_jattach_arg = "$(location %s)" % px_jattach

stirling_profiler_attach_args = [
    "-stirling_profiler_px_jattach_path %s" % px_jattach_arg,
]

stirling_profiler_java_args = [
    "-stirling_profiler_java_agent_libs %s" % agent_libs_arg,
    "-stirling_profiler_px_jattach_path %s" % px_jattach_arg,
]

jdk_names = [
    "java_image_base",
    "azul-zulu",
    "azul-zulu-debian",
    "azul-zulu-alpine",
    "amazon-corretto",
    "adopt-j9",
    "ibm",
    "sap",
    "graal-vm",
]
