#!/bin/bash

# TODO(oazizi): Consider splitting into two files. One for general utils, one for stirling specific test utils.

# If not root, this command runs the provided command through sudo.
# Meant to be used interactively when not root, since sudo will prompt for password.
run_prompt_sudo() {
  cmd=$1
  shift

  if [[ $EUID -ne 0 ]]; then
     if [[ $(stat -c '%u' "$(command -v sudo)") -eq 0 ]]; then
        sudo "$cmd" "$@"
      else
        echo "ERROR: Cannot run as root"
      fi
  else
     "$cmd" "$@"
  fi
}

# Thus function checks the output of stirling in --init_mode for errors or warnings.
# Used by tests.
check_stirling_output() {
  out=$1
  echo "$out"

  # Look for GLOG errors or warnings, which start with E or W respectively.
  err_count=$(echo "$out" | grep -c -e ^E -e ^W || true)
  echo "Error/Warning count = $err_count"
  if [ "$err_count" -ne "0" ]; then
    echo "Test FAILED"
    return 1
  fi

  success_msg=$(echo "$out" | grep -c -e "Probes successfully deployed" || true)
  if [ "$success_msg" -ne 1 ]; then
    echo "Could not find success message"
    echo "Test FAILED"
    return 1
  fi

  echo "Test PASSED"
  return 0
}
