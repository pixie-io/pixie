#!/usr/bin/env -S -i /bin/bash

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

# shellcheck shell=bash

get_free_port() {
  read -r l u < /proc/sys/net/ipv4/ip_local_port_range
  for port in $(seq "${l}" "${u}"); do
    if ! (echo "" 2>/dev/null > "/dev/tcp/127.0.0.1/${port}"); then
      echo "${port}"
      break
    fi
  done
}

export OLDPWD="$PWD"
export RUNFILES_DIR="$PWD"
export INTERACTIVE_MODE="true"
SSH_PORT="$(get_free_port)"
export SSH_PORT

priv_key="${RUNFILES_DIR}/qemu_guest_ssh_key"
pub_key="${priv_key}.pub"
rm "${priv_key}" &> /dev/null || true
rm "${pub_key}" &> /dev/null || true
ssh-keygen -f "${priv_key}" -N '' -b 1024 &>/dev/null

export SSH_PUB_KEY="${pub_key}"

log="$PWD/interactive_qemu.log"
# shellcheck disable=SC2288
%qemu_runner_path% &> "${log}" &
qemu_pid="$!"

script_pid="$$"

cleanup() {
  set +e
  echo "Killing QEMU"
  kill "${qemu_pid}"
  while kill -0 "${qemu_pid}" 2>/dev/null; do
    sleep 1
  done
  exit
}

trap cleanup SIGINT

echo "QEMU logs redirected to ${log}"
while ! ssh -t -i "${priv_key}" -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -p "${SSH_PORT}" root@localhost 'cd /test_fs; bash -l'; do
  sleep 1
done

cleanup
