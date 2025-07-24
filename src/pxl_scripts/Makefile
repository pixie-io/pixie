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

# Update dir name here if you want to add a new directory.
dirs := bpftrace px pxbeta sotw
script_files := $(foreach dir,$(dirs),$(wildcard $(dir)/**/*))

EXECUTABLES = px
K := $(foreach exec,$(EXECUTABLES),\
	$(if $(shell which $(exec)),some string,$(error "No $(exec) in PATH")))

# Optional path prefix to use for the script_files path. Used by bazel
# to ensure the scripts are in the correct location.
PATH_PREFIX ?= ""
PX_BIN ?= px

all: bundle-oss.json.gz

bundle-oss.json: $(script_files)

	@# When run in CI, $HOME may not be set. This ensures that the
	@# px create-bundle command can run successfully.
	@if [ -z "$$HOME" ]; then \
		TMPDIR=$$(mktemp -d); \
		export HOME=$$TMPDIR; \
		trap "rm -rf $$TMPDIR" EXIT; \
	fi; \
	$(PX_BIN) create-bundle --search_path $(PWD) $(foreach dir,$(dirs),--base $(PATH_PREFIX)$(dir)) -o $(PWD)/bundle-oss.json

bundle-oss.json.gz: bundle-oss.json
	gzip -c $< > $@

update_readme:
	./update_readme.py . https://github.com/pixie-io/pixie/tree/main/src/pxl_scripts

.PHONY: dev
dev:
	./watch.sh
