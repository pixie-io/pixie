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

# Commands.
GSUTIL := gsutil
SHA256SUM := sha256sum

# Variables
DIST ?= bullseye
OUTDIR ?= out

ts := $(shell date +'%Y%m%d%H%M%S')
sysroot_base_path :=  gs://pixie-dev-public/sysroots
sysroot_path := $(sysroot_base_path)/$(ts)

.PHONY:
upload_sysroots: upload_$(DIST)_arm64_sysroot upload_$(DIST)_amd64_sysroot

.PHONY:
upload_$(DIST)_%_sysroot: $(OUTDIR)/sysroot_$(DIST)_%.tar.gz $(OUTDIR)/sysroot_$(DIST)_%.tar.gz.sha256
	$(GSUTIL) cp $< $(sysroot_path)/$<
	$(GSUTIL) cp $<.sha256 $(sysroot_path)/$<.sha256

	@echo "SHA: $(shell cat $<.sha256)"


$(OUTDIR)/sysroot_$(DIST)_%.tar.gz: sysroot_creator_$(DIST).sh sysroot_creator.sh
	./$< -a $* -o $@

%.sha256: %
	$(SHA256SUM) $< > $@
