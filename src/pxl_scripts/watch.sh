#!/usr/bin/env bash

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

shell_join() {
  local arg
  printf "%s" "$1"
  shift
  for arg in "$@"; do
    printf " "
    printf "%s" "${arg// /\ }"
  done
}

# String formatting functions.
if [[ -t 1 ]]; then
  tty_escape() { printf "\033[%sm" "$1"; }
else
  tty_escape() { :; }
fi

tty_mkbold() { tty_escape "1;$1"; }
tty_cyan="$(tty_mkbold 36)"
tty_reset="$(tty_escape 0)"
tty_bold="$(tty_mkbold 39)"
tty_green="$(tty_mkbold 32)"

emph() {
  printf "${tty_cyan}==>${tty_bold} %s${tty_reset}\n" "$(shell_join "$@")"
}

SERVER_PID=""

cleanup() {
  if [[ -n "${SERVER_PID}" ]]; then
    kill "${SERVER_PID}"
  fi
  emph "Cleanup"
  echo "Run ${tty_green}localStorage.clear('px-custom-bundle-paths')${tty_reset}"
}

python3 cors_http_server.py &
SERVER_PID=$!

trap 'cleanup' EXIT

emph "Running dev server for pxl_scripts"
echo "Open chrome console and add: "\
     "${tty_green}localStorage.setItem('px-custom-bundle-paths',"\
     "'["'"'"http://127.0.0.1:8000/bundle-oss.json"'"'"]')${tty_reset}"

while sleep 1; do
    make -s bundle-oss.json
done
