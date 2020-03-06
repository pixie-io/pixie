#!/usr/bin/env bash
################################################################
# PIXIE Installer
# This is inspired by the homebrew installer.
################################################################
set -u

CLOUD_ADDR="withpixie.ai"
INSTALL_PATH=/usr/local/bin
ARTIFACT_BASE_PATH="https://storage.googleapis.com/pixie-prod-artifacts/cli"
ARTIFACT_NAME=cli_darwin_amd64
USE_VERSION=latest
USER_INSTALL_PATH="$HOME/bin"

# First check if the OS is Linux.
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

${tty_bold}Usage:${tty_reset} $0 [-v <version>]

EOS
    exit 1
}

while getopts ":v:c:h" o; do
    case "${o}" in
        v)
            USE_VERSION=${OPTARG}
            ;;
        c)
            CLOUD_ADDR=${OPTARG}
            ;;
        h)
            usage
            ;;
        *)
            usage
            ;;
    esac
done
shift $((OPTIND-1))


artifact_url() {
  echo "${ARTIFACT_BASE_PATH}/${USE_VERSION}/${ARTIFACT_NAME}"
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
  read -s -r -n 1 -p "Continue (Y/n): " c
  # We test for \r and \n because some stuff does \r instead.
  if ! [[ "$c" == '' || "$c" == $'\r' || "$c" == $'\n' || "$c" == 'Y' || "$c" == 'y' ]]; then
    exit 1
  fi
  echo
}

exists_but_not_writable() {
  [[ -e "$1" ]] && ! [[ -r "$1" && -w "$1" && -x "$1" ]]
}


if exists_but_not_writable "${INSTALL_PATH}"; then
    if exists_but_not_writable "${USER_INSTALL_PATH}"; then
        abort "Neither ${INSTALL_PATH} or ${USER_INSTALL_PATH} are writable."
    fi
    echo "${tty_yellow}PATH: ${INSTALL_PATH} is not writable, will"\
         "install in user install path: ${USER_INSTALL_PATH}${tty_reset}"
    INSTALL_PATH="${USER_INSTALL_PATH}"
fi

emph "INSTALL INFO:"
cat << EOS
The PX CLI will be installed to:
- ${tty_bold}Install PATH:${tty_reset} ${INSTALL_PATH}
EOS
wait_for_user

# TODO(zasgar): Check to make sure PX does not already exist, and if it does if it's actually pixie.
# TODO(zasgar): Check the sha256.
execute curl -fSL "$(artifact_url)" -o "${INSTALL_PATH}"/px
execute chmod +x "${INSTALL_PATH}"/px

echo
emph "Authenticating with Pixie Cloud:"


if ! "${INSTALL_PATH}"/px auth login --cloud_addr "${CLOUD_ADDR}" -q; then
cat << EOS

${tty_red}FAILED to authenticate with Pixie cloud. ${tty_reset}
  You can try this step yourself by running ${tty_green}px auth login${tty_reset}.
  For help, please contact support@pixielabs.ai or join our community slack/github"

EOS
fi

echo
emph "Next steps:"
cat << EOS
- PX CLI has been installed to: ${INSTALL_PATH}. Make sure this directory is in your PATH.
- Run ${tty_green}px deploy${tty_reset} to deploy pixie on K8s.
- Run ${tty_green}px help${tty_reset} to get started, or visit our UI: ${tty_underline}https://${CLOUD_ADDR}${tty_reset}
- Further documentation:
    ${tty_underline}https://${CLOUD_ADDR}/docs${tty_reset}
EOS
