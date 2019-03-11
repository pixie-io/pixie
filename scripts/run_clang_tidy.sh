#!/bin/bash

# We can have the Clang tidy script in a few different places. Check them in priority
# order.
clang_tidy_scripts=(
    "/opt/clang-7.0/share/clang/run-clang-tidy.py"
    "/usr/local/opt/llvm/share/clang/run-clang-tidy.py")

clang_tidy_script=""
for script_option in "${clang_tidy_scripts[@]}"
do
    echo $script_option
    if [ -f "${script_option}" ]; then
        clang_tidy_script=${script_option}
        break
    fi
done

if [ -z "${clang_tidy_script}" ]; then
    echo "Failed to find a valid clang tidy script runner (check LLVM/Clang install)"
    exit 1
fi

echo "Selected: ${clang_tidy_script}"

set -e
SRCDIR=$(bazel info workspace)
echo "Generating compilation database..."
pushd $SRCDIR
# Bazel build need to be run to setup virtual includes, generating files which are consumed
# by clang-tidy.
"${SRCDIR}/scripts/gen_compilation_database.py" --include_headers --run_bazel_build

# Actually invoke clang-tidy.
"${clang_tidy_script}" -j $(nproc)
popd
