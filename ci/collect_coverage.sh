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

# We use GCC for building coverage code.
export CC=gcc
export CXX=g++

GIT_REPO=""
GIT_COMMIT=""
GIT_BRANCH=""
CODECOV_TOKEN="${CODECOV_TOKEN:-""}"
GENERATE_HTML=false
UPLOAD_TO_CODECOV=false
HTML_OUTPUT_DIR=""

COVERAGE_FILE="coverage.info"

# Print out the usage information and exit.
usage() {
  echo "Usage $0 [-u] [-g] [-t <codecov_token>] [-c <git_commit>] [-b <git_branch>] [-o <output_dir>] [-r <git_repo>]" 1>&2;
  echo "   -u    Upload to CodeCov. Requires -t, -c, -b, -r"
  echo "   -g    Generate LCOV html. Requires -o"
  exit 1;
}

# Print out the config information.
print_config() {
  echo "Config: "
  echo "  Upload to CodeCov: ${UPLOAD_TO_CODECOV}"
  echo "  GIT_REPO         : ${GIT_REPO}"
  echo "  GIT_COMMIT       : ${GIT_COMMIT}"
  echo "  GIT_BRANCH       : ${GIT_BRANCH}"
  echo "  CODECOV_TOKEN    : ${CODECOV_TOKEN}"
  echo "  Generate HTML    : ${GENERATE_HTML}"
  echo "  HTML_OUTPUT_DIR  : ${HTML_OUTPUT_DIR}"
}

check_config() {
  if [ "${UPLOAD_TO_CODECOV}" = true ]; then
    if [ "${GIT_REPO}" = "" ]; then
      echo "Option -r to specify git commit is required wih -u"
      exit 1
    fi
    if [ "${GIT_COMMIT}" = "" ]; then
      echo "Option -c to specify git commit is required wih -u"
      exit 1
    fi
    if [ "${GIT_BRANCH}" = "" ]; then
      echo "Option -b to specify git branch is required wih -u"
      exit 1
    fi
    if [ "${CODECOV_TOKEN}" = "" ]; then
      echo "Option -t to specify codecov token is required wih -u"
      exit 1
    fi
  fi

  if [ "${GENERATE_HTML}" = true ]; then
    if [ "${HTML_OUTPUT_DIR}" = "" ]; then
      echo "Option -o to specify html output is required wih -g"
      exit 1
    fi
  fi
}

parse_args() {
  local OPTIND
  # Process the command line arguments.
  while getopts "guc:b:t:o:r:h" opt; do
    case ${opt} in
      g)
        GENERATE_HTML=true
        ;;
      u)
        UPLOAD_TO_CODECOV=true
        ;;
      c)
        GIT_COMMIT=$OPTARG
        ;;
      b)
        GIT_BRANCH=$OPTARG
        ;;
      t)
        CODECOV_TOKEN=$OPTARG
        ;;
      o)
        HTML_OUTPUT_DIR=$OPTARG
        ;;
      r)
        GIT_REPO=$OPTARG
        ;;
      :)
        echo "Invalid option: $OPTARG requires an argument" 1>&2
        ;;
      h)
        usage
        ;;
      *)
        usage
        ;;
    esac
  done
  shift $((OPTIND -1))
}

generate_html() {
  genhtml -o "${HTML_OUTPUT_DIR}" -s ${COVERAGE_FILE}
}

upload_to_codecov() {
  codecov -t "${CODECOV_TOKEN}" -B "${GIT_BRANCH}" -C "${GIT_COMMIT}" -r "${GIT_REPO}" -f "${COVERAGE_FILE}"
}

# We use globs, make sure they are supported.
shopt -s globstar

# Parse the input arguments.
parse_args "$@"

# Check config parameters.
check_config

# Print config parameters.
print_config

lcov_opts=(--rc lcov_branch_coverage=1)

cd $(bazel info workspace)

# Get coverage from bazel targets.
bazel coverage --remote_download_outputs=all --combined_report=lcov //src/...

# Copy the output file
cp --no-preserve=mode "$(bazel info output_path)/_coverage/_coverage_report.dat" ${COVERAGE_FILE}

# Print out the summary.
lcov "${lcov_opts[@]}" --summary ${COVERAGE_FILE}

# Remove test files from the coverage files.
lcov "${lcov_opts[@]}" -r ${COVERAGE_FILE} '**/*_test.cc' -o ${COVERAGE_FILE}
lcov "${lcov_opts[@]}" -r ${COVERAGE_FILE} '**/*_mock.cc' -o ${COVERAGE_FILE}
lcov "${lcov_opts[@]}" -r ${COVERAGE_FILE} '**/*_mock.h' -o ${COVERAGE_FILE}
lcov "${lcov_opts[@]}" -r ${COVERAGE_FILE} '**/*_test.go' -o ${COVERAGE_FILE}
lcov "${lcov_opts[@]}" -r ${COVERAGE_FILE} '**/*.gen.go' -o ${COVERAGE_FILE}
lcov "${lcov_opts[@]}" -r ${COVERAGE_FILE} '**/*-mock.tsx' -o ${COVERAGE_FILE}
lcov "${lcov_opts[@]}" -r ${COVERAGE_FILE} '**/*-mock.ts' -o ${COVERAGE_FILE}
lcov "${lcov_opts[@]}" -r ${COVERAGE_FILE} 'src/ui/src/types/generated/**' -o ${COVERAGE_FILE}

# Print out the final summary.
lcov "${lcov_opts[@]}" --summary ${COVERAGE_FILE}

# Upload to codecov.io.
if [ "${UPLOAD_TO_CODECOV}" = true ]; then
  upload_to_codecov
fi

# Generate HTML.
if [ "${GENERATE_HTML}" = true ]; then
  generate_html
fi
