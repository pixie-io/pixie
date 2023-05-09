#!/bin/bash
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

set -e

function check_env_set() {
  if [[ -z "${!1}" ]]; then
    echo "The environment variable \"$1\" needs to be set"
    exit 1
  fi
}

check_env_set QEMU_TEST_FS_PATH
check_env_set QEMU_KERNEL_IMAGE
check_env_set QEMU_DISK_BASE_RO
check_env_set MONITOR_SOCK

QEMU_MEMORY=${QEMU_MEMORY:-4096M}
QEMU_CPU_COUNT=${QEMU_CPU_COUNT:-2}

# This needs to match what is in the exit c file.
QEMU_EXIT_BASE="0xf4"
QEMU_USE_KVM=${QEMU_USE_KVM:-true}

# Create a r/w overlay disk image.
overlay_disk_image=$(mktemp --suffix .qcow2)
qemu-img create -f qcow2 -F qcow2 \
	 -b "${QEMU_DISK_BASE_RO}" "${overlay_disk_image}"

flags=()

if [[ "${QEMU_USE_KVM}" = true ]]; then
  flags+=(-enable-kvm)
fi

# System config:
flags+=(-machine "pc,usb=off,dump-guest-core=off")
# We have to change this if we plan to test ARM on X86, etc.
flags+=(-cpu host)
flags+=(-m "${QEMU_MEMORY}")
if [[ "${QEMU_CPU_COUNT}" -gt 1 ]]; then
  flags+=(-smp "${QEMU_CPU_COUNT}")
fi

# Use random device from host.
flags+=(-object "rng-random,id=rng0,filename=/dev/urandom")
flags+=(-device "virtio-rng-pci,rng=rng0")

# Use localtime, vm clock, don't fix drift because it might break some sensitive tests.
flags+=(-rtc "base=localtime,clock=vm,driftfix=none")

# Disk mounts:
flags+=(-hda "${overlay_disk_image}")
flags+=(-virtfs "local,path=${QEMU_TEST_FS_PATH},mount_tag=test_fs,security_model=mapped")

# Exit device:
flags+=(-device "isa-debug-exit,iobase=${QEMU_EXIT_BASE},iosize=0x8")

# Kernel config:
flags+=(-kernel "${QEMU_KERNEL_IMAGE}")
flags+=(-append "console=ttyS0 root=/dev/sda")

# Disable graphics mode.
flags+=(-nographic)

# Enable ssh port forwarding.
flags+=(-device "virtio-net-pci,netdev=net0")
flags+=(-netdev "user,id=net0,hostfwd=tcp::0-:22")

flags+=(-monitor "unix:${MONITOR_SOCK},server,nowait")

retval=0
exec qemu-system-x86_64 "${flags[@]}" || retval=$?

if [[ "${retval}" -gt 0 ]]; then
    if [[ "${retval}" -lt 128 ]]; then
	    echo "QEMU failed to launch with status code: ${retval}"
    else
	    retval="$(echo "($retval-128-1)/2" | bc)"
    fi
fi

if [[ "${retval}" -ne 0 ]]; then
  echo "Running sshd inside qemu failed with status: ${retval}"
fi

exit "${retval}"
