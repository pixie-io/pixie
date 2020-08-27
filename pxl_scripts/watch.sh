#!/usr/bin/env bash

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
  echo "Run ${tty_green}localStorage.clear('px-custom-bundle-path')${tty_reset}"
}

python3 cors_http_server.py &
SERVER_PID=$!

trap 'cleanup' EXIT

emph "Running dev server for pxl_scripts"
echo "Open chrome console and add: "\
     "${tty_green}localStorage.setItem('px-custom-oss-bundle-path',"\
     "'http://127.0.0.1:8000/bundle-oss.json')${tty_reset}"

while sleep 1; do
    make -s bundle-oss.json
done
