#!/bin/bash -e

# TODO(oazizi): Consider moving this to somewhere under common.

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

# This function builds the bazel target and returns the path to the output, relative to ToT.
function bazel_build() {
  target=$1
  flags=$2
  # shellcheck disable=SC2086
  bazel_out=$(bazel build $flags "$target" 2>&1)
  retval=$?

  # If compile failed, log and abort.
  if [ $retval -ne 0 ]; then
    echo "$bazel_out" > /tmp/bazel_build.out
    # Echo to stderr, so it gets printed even when invoked inside a sub-shell.
    >&2 echo "Failed to build stirling_wrapper. See logs in /tmp/bazel_build.out."
    return $retval
  fi

  # This funky command looks for and parses out the binary from a output like the following:
  # ...
  # Target //src/stirling/binaries:stirling_wrapper up-to-date:
  #   bazel-bin/src/stirling/binaries/stirling_wrapper
  # ...
  binary=$(echo "$bazel_out" | grep -A1 "^Target .* up-to-date" | tail -1 | sed -e 's/^[[:space:]]*//')
  echo "$binary"
}

function docker_stop() {
  container_name=$1
  echo "Stopping container $container_name ..."
  docker container stop "$container_name"
}

function docker_load() {
  image=$1
  docker load -i "$image" | awk '/Loaded image/ {print $NF}'
}
