#!/bin/bash -e

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

if [[ "$#" -lt 1 ]]; then
  echo "Usage: $0 <server_ip>"
  exit 1
fi

server_ip="$1"
disks=(/dev/nvme0n1 /dev/nvme1n1)
mount_dir="/data"

{
cat <<EOF
set -ex
mdadm --create --verbose /dev/md0 --level=0 --raid-devices="${#disks[@]}" ${disks[@]}
mkfs -t ext4 /dev/md0
mdadm --detail --scan >> /etc/mdadm/mdadm.conf

mkdir -p "${mount_dir}"
mount /dev/md0 "${mount_dir}"

echo "/dev/md0 ${mount_dir} ext4 defaults 0 0" >> /etc/fstab

update-initramfs -u
EOF
} | ssh "root@${server_ip}" '/bin/bash -s'
