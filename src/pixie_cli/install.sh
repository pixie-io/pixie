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

################################################################
# PIXIE Installer
# This is inspired by the homebrew installer.
################################################################
set -u

DEFAULT_INSTALL_PATH=/usr/local/bin
ARTIFACT_NAME=cli_darwin_universal
USER_INSTALL_PATH="$HOME/bin"
GITHUB_REPO="pixie-io/pixie"

PIXIE_BANNER="
  ___  _       _
 | _ \(_)__ __(_) ___
 |  _/| |\ \ /| |/ -_)
 |_|  |_|/_\_\|_|\___|
"

ARTIFACT_BASE_PATH="https://github.com/${GITHUB_REPO}/releases/download/release%2Fcli%2Fv"

# Fetch latest version.
LATEST_VERSION=""
regex='v(([0-9]+)\.([0-9]+)\.([0-9]+))\)<!--cli-latest-release-->'
if [[ "$(curl -fsSL https://raw.githubusercontent.com/pixie-io/pixie/main/README.md)" =~ $regex ]]; then
    LATEST_VERSION=${BASH_REMATCH[1]}
fi

USE_VERSION=${PL_CLI_VERSION:-${LATEST_VERSION}}
USE_VERSION=${PX_CLI_VERSION:-${USE_VERSION}}

# Check if the OS is Linux.
if [[ "$(uname)" = "Linux" ]]; then
    ARTIFACT_NAME=cli_linux_amd64
fi

# String formatting functions.
if [[ -t 1 ]]; then
  tty_escape() { printf "\033[%sm" "$1"; }
else
  tty_escape() { :; }
fi

tty_mkbold() { tty_escape "1;$1"; }
tty_underline="$(tty_escape "4;39")"
tty_cyan="$(tty_mkbold 36)"
tty_yellow="$(tty_mkbold 33)"
tty_green="$(tty_mkbold 32)"
tty_red="$(tty_mkbold 31)"
tty_bold="$(tty_mkbold 39)"
tty_reset="$(tty_escape 0)"

# Trap ctrl-c and call ctrl_c() to reset terminal.
trap ctrl_c INT

function ctrl_c() {
    stty sane
    exit
}

# Parse Options:
#TODO(zasgar): Better usage.
usage() {
    cat << EOS

${tty_bold}Usage:${tty_reset} $0

EOS
    exit 1
}

while getopts ":v:c:h" o; do
    case "${o}" in
        h)
            usage
            ;;
        *)
            usage
            ;;
    esac
done
shift $((OPTIND-1))

print_dev_message() {
  if [[ -n "${PL_TESTING_ENV:-}" ]]; then
    emph_red "${tty_red}IN DEVELOPMENT MODE: PL_TESTING_ENV=${PL_TESTING_ENV},"\
              "PL_CLI_VERSION=${PL_CLI_VERSION:-}, PL_VIZIER_VERSION=${PL_VIZIER_VERSION:-}"\
              "${tty_reset}"
  fi
}

artifact_url() {
  echo "${ARTIFACT_BASE_PATH}${USE_VERSION}/${ARTIFACT_NAME}"
}

artifact_sha_url() {
  echo "${ARTIFACT_BASE_PATH}${USE_VERSION}/${ARTIFACT_NAME}.sha256"
}

have_sudo_access() {
  if [[ -z "${HAVE_SUDO_ACCESS-}" ]]; then
    /usr/bin/sudo -l mkdir &>/dev/null
    HAVE_SUDO_ACCESS="$?"
  fi

  return "$HAVE_SUDO_ACCESS"
}

shell_join() {
  local arg
  printf "%s" "$1"
  shift
  for arg in "$@"; do
    printf " "
    printf "%s" "${arg// /\ }"
  done
}

emph_red() {
  printf "${tty_red}==>${tty_bold} %s${tty_reset}\n" "$(shell_join "$@")"
}

emph() {
  printf "${tty_cyan}==>${tty_bold} %s${tty_reset}\n" "$(shell_join "$@")"
}

abort() {
  printf "%s\n" "$1"
  exit 1
}

execute() {
  if ! "$@"; then
    abort "$(printf "Failed during: %s" "$(shell_join "$@")")"
  fi
}

wait_for_user() {
  local c
  echo
  read -r -p "Continue (Y/n): " c
  # We test for \r and \n because some stuff does \r instead.
  if ! [[ "$c" == '' || "$c" == $'\r' || "$c" == $'\n' || "$c" == 'Y' || "$c" == 'y' ]]; then
    exit 1
  fi
  echo
}

exists_but_not_writable() {
  [[ -e "$1" ]] && ! [[ -r "$1" && -w "$1" && -x "$1" ]]
}

print_dev_message

if exists_but_not_writable "${DEFAULT_INSTALL_PATH}"; then
    DEFAULT_INSTALL_PATH=${USER_INSTALL_PATH}
fi

echo "${tty_green}${PIXIE_BANNER}${tty_reset}"

emph "Info:"
cat << EOS
Pixie gives engineers access to no-instrumentation, streaming &
unsampled auto-telemetry to debug performance issues in real-time,
More information at: ${tty_underline}https://www.px.dev${tty_reset}.

This command will install the Pixie CLI (px) in a location selected
by you, and performs authentication with Pixie's control plane.
After installation of the CLI you can easily manage Pixie
installations on your K8s clusters and execute scripts to collect
telemetry from your clusters using Pixie.

Docs:
  ${tty_underline}https://docs.px.dev${tty_reset}
Github:
  ${tty_underline}https://github.com/pixie-io/pixie${tty_reset}
EOS


printf "\n\n"
emph "Terms and Conditions ${tty_underline}https://www.px.dev/terms${tty_reset}"
read -r -p "I have read and accepted the Terms & Conditions [y/n]: " READ_TERMS
printf "\n\n"

READ_TERMS=${READ_TERMS:0:1}
if ! [[ "$READ_TERMS" == 'Y' || "$READ_TERMS" == 'y' ]]; then
    abort "Cannot install Pixie CLI (px) until you accept the Terms & Conditions."
fi

emph "Installing PX CLI:"
read -r -p "Install Path [${DEFAULT_INSTALL_PATH}]: " INSTALL_PATH
INSTALL_PATH=${INSTALL_PATH:-${DEFAULT_INSTALL_PATH}}

if [[ "$INSTALL_PATH" != /* ]]
then
  abort "Install Path must be absolute path: [/xxx]"

fi
if exists_but_not_writable "${INSTALL_PATH}"; then
    abort "${INSTALL_PATH} is not writable or does not exist."
fi

if [[ ! -e "${INSTALL_PATH}" ]]; then
    if ! mkdir -p "${INSTALL_PATH}"; then
        abort "Failed to create directory: ${INSTALL_PATH}"
    fi
fi

# TODO(zasgar): Check to make sure PX does not already exist, and if it does if it's actually Pixie.
# Note: we need this download, mv step to make sure macos does not mark this binary as bad.
artifact_sha=$(curl -fsSL "$(artifact_sha_url)")
execute curl -fsSL "$(artifact_url)" -o "${INSTALL_PATH}"/px_new
actual_sha=$(shasum -a 256 "${INSTALL_PATH}"/px_new | head -c 64)
if [[ "$artifact_sha" != "$actual_sha" ]]; then
    abort "SHA for downloaded CLI does not match expected SHA"
fi
execute chmod +x "${INSTALL_PATH}"/px_new
execute mv "${INSTALL_PATH}"/px_new "${INSTALL_PATH}"/px

echo
emph "Next steps:"
cat << EOS
- PX CLI has been installed to: ${INSTALL_PATH}. Make sure this directory is in your PATH.
- First run ${tty_green}px auth login${tty_reset} with \`PX_CLOUD_ADDR\` set to authenticate.
- Run ${tty_green}px deploy${tty_reset} to deploy Pixie on K8s.
- Run ${tty_green}px help${tty_reset} for more commands.
- Further documentation:
    ${tty_underline}docs.px.dev${tty_reset}
EOS
