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

BUNDLE := gs://pixie-prod-artifacts/script-bundles/bundle-oss.json

# Update dir name here if you want to add a new directory.
dirs := px pxbeta sotw
script_files := $(foreach dir,$(dirs),$(wildcard $(dir)/**/*))

EXECUTABLES = px
K := $(foreach exec,$(EXECUTABLES),\
	$(if $(shell which $(exec)),some string,$(error "No $(exec) in PATH")))

all: bundle-oss.json.gz

bundle-oss.json: $(script_files)
	px create-bundle --search_path $(PWD) $(foreach dir,$(dirs),--base $(dir)) -o $(PWD)/bundle-oss.json

bundle-oss.json.gz: bundle-oss.json
	gzip -c $< > $@

.PHONY: update_bundle
update_bundle: bundle-oss.json.gz
# Requires prod access to update, or needs to run in CI deploy.
	gsutil -h "Cache-Control:no-cache,max-age=0" \
	       -h "Content-Type:application/json" \
	       -h "Content-Encoding: gzip" \
	       cp $< $(BUNDLE)
# Readable by everyone
	gsutil acl ch -u allUsers:READER $(BUNDLE)

update_readme:
	./update_readme.py . https://github.com/pixie-labs/pixie/tree/main/src/pxl_scripts

.PHONY: dev
dev:
	./watch.sh
