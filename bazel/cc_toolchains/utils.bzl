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

# abi returns the expected abi for the given arch libc_version pair.
# Currently, doesn't support musl.
def abi(arch, libc_version):
    # Technically, the abi for aarch64 is eabi, however, all of the debian aarch64 sysroots we use have their files in aarch64-linux-gnu.
    # So we treat aarch64 as having an abi of gnu as well.
    if libc_version.startswith("glibc"):
        return "gnu"

    # We should support musl in the future.
    fail("Cannot determine abi from arch ({arch}) and libc version ({libc_version})".format(
        arch = arch,
        libc_version = libc_version,
    ))
