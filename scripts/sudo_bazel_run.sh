#!/bin/bash -e

usage() {
  echo "This script effectively executes 'bazel run' with sudo"
  echo ""
  echo "Usage: $0 <bazel_flags> <bazel_target> [-- <arguments to binary>]"
  echo ""
  echo "Example:"
  echo "  sudo_bazel_run.sh -c opt //src/stirling:socket_trace_bpf_test -- --gtest_filter=Test.Test"
  echo "  if you wanted to run the following as sudo (which won't work)"
  echo "  bazel run -c opt //src/stirling:socket_trace_bpf_test -- --gtest_filter=Test.Test"

  exit
}

if [ $# -eq 0 ]; then
  usage
fi

if [ "$1" == "-h" ]; then
  usage
fi

# Assuming script is installed in $PIXIE_ROOT/bin
script_dir="$(dirname "$0")"
cd "$script_dir"/.. || exit

# Extract bazel build args and target.
# But leave runtime arguments in the array.
for x in "$@"; do
  # Look for the argument separator.
  if [[ "$x" == "--" ]]; then
    shift
    break;
  fi
  build_args+=("$x")
  shift
done

options=("${build_args[@]::${#build_args[@]}-1}")
target="${build_args[-1]}"
run_args=("${@}")

echo "Bazel options: ${options[*]}"
echo "Bazel target: $target"
echo "Run args: ${run_args[*]}"

# Perform the build as user (not as root).
bazel build "${options[@]}" "$target"

# Now find the bazel-bin directory.
bazel_bin=$(bazel info "${options[@]}" bazel-bin 2> /dev/null)

# Modify the target from //dir/subdir:exe to ${bazel_bin}/dir/subdir/exe.
target=${target//":"/"/"}
target=${target//"//"/""}
target=${bazel_bin}/${target}

# Run the binary with sudo.
sudo "$target" "${run_args[@]}"
