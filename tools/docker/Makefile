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
DOCKER := docker
BUILD_DIR ?= .build

# We use our dev image to build some other tools. This fetches the latest tag.
TOT		       := $$(git rev-parse --show-toplevel)
DOCKER_PROPERTIES_FILE := $(TOT)/docker.properties

GH_RELEASE_UPLOAD := $(TOT)/scripts/create_release_for_dev_artifact.sh

DEV_EXTRAS_TAG = $$(grep DOCKER_IMAGE_TAG $(DOCKER_PROPERTIES_FILE) | cut -d'=' -f2)
DEV_EXTRAS_DIGEST = $$(grep DEV_IMAGE_WITH_EXTRAS_DIGEST $(DOCKER_PROPERTIES_FILE) | cut -d'=' -f2)
DEV_EXTRAS_IMAGE = "gcr.io/pixie-oss/pixie-dev-public/dev_image_with_extras:$(DEV_EXTRAS_TAG)"

## Clang deb parameters
CLANG_VERSION := 15.0
CLANG_SUFFIX := pl12

CLANG_TAG="$(CLANG_VERSION)-$(CLANG_SUFFIX)"

clang_deb_fname := "clang-$(CLANG_TAG).deb"
clang_linters_deb_fname := "clang-linters-$(CLANG_TAG).deb"
clang_gs_path :=  gs://pixie-dev-public/clang/$(CLANG_TAG)
clang_deb_image_tag := "gcr.io/pixie-oss/pixie-dev-public/clang_deb_builder_image:$(CLANG_VERSION)"
CLANG_BUILD_DIR := "$(BUILD_DIR)/clang-$(CLANG_TAG)"

## gperftools parameters
GPERFTOOLS_DEB_IMAGE_VERSION := 2.10-pl1
gperftools_deb_fname := gperftools-pixie-$(GPERFTOOLS_DEB_IMAGE_VERSION).deb
gperftools_deb_gs_path :=  gs://pixie-dev-public/$(gperftools_deb_fname)
gperftools_deb_image_tag := "gcr.io/pixie-oss/pixie-dev-public/gperftools_deb_image:$(GPERFTOOLS_DEB_IMAGE_VERSION)"

## graalvm parameters
GRAALVM_IMAGE_VERSION := pl1
GRAALVM_ARCHIVE_IMAGE_TAG := graalvm-$(GRAALVM_IMAGE_VERSION)
GRAALVM_ARCHIVE_FNAME := graalvm-native-image-22.3.0.tar.gz
GRAALVM_ARCHIVE_GS_PATH := gs://pixie-dev-public/graalvm-native-image-22.3.0-$(GRAALVM_IMAGE_VERSION).tar.gz

## Sysroot parameters
SYSROOT_REV := pl8
SYSROOT_BUILD_DIR := $(BUILD_DIR)/sysroots
SYSROOT_ARCHITECTURES := amd64 arm64
SYSROOT_VARIANTS := runtime build test
SYSROOT_UPLOAD_TARGET_TEMPLATE := upload_sysroot_arch_variant
SYSROOT_TARGETS = $(patsubst variant,$(patsubst arch,$(SYSROOT_UPLOAD_TARGET_TEMPLATE, $(SYSROOT_ARCHITECTURES)), $(SYSROOT_VARIANTS)))
SYSROOT_GS_PATH := gs://pixie-dev-public/sysroots/$(SYSROOT_REV)
SYSROOT_CREATOR_IMAGE_TAG := sysroot-creator-$(SYSROOT_REV)

## Linux image parameters
LINUX_HEADER_BUILD_DIR := $(BUILD_DIR)/linux_headers
LINUX_HEADER_ASSETS_BUILD_DIR := $(LINUX_HEADER_BUILD_DIR)/assets
LINUX_KERNEL_VERSIONS := 4.14.304 \
	4.15.18 \
	4.16.18 \
	4.17.19 \
	4.18.20 \
	4.19.271 \
	4.20.17 \
	5.0.21 \
	5.1.21 \
	5.2.21 \
	5.3.18 \
	5.4.230 \
	5.5.19 \
	5.6.19 \
	5.7.19 \
	5.8.18 \
	5.9.16 \
	5.10.165 \
	5.11.22 \
	5.12.19 \
	5.13.19 \
	5.14.21 \
	5.15.90 \
	5.16.20 \
	5.17.15 \
	5.18.19 \
	5.19.17 \
	6.0.19 \
	6.1.8

LINUX_HEADER_X86_64_TEMPLATE := linux-headers-x86_64-%.tar.gz
LINUX_HEADER_ARM64_TEMPLATE := linux-headers-arm64-%.tar.gz
LINUX_HEADER_X86_64_TARGETS = $(addprefix $(LINUX_HEADER_ASSETS_BUILD_DIR)/, $(patsubst %,$(LINUX_HEADER_X86_64_TEMPLATE), $(LINUX_KERNEL_VERSIONS)))
LINUX_HEADER_ARM64_TARGETS = $(addprefix $(LINUX_HEADER_ASSETS_BUILD_DIR)/, $(patsubst %,$(LINUX_HEADER_ARM64_TEMPLATE), $(LINUX_KERNEL_VERSIONS)))
LINUX_HEADERS_X86_64_MERGED_FILE := $(LINUX_HEADER_BUILD_DIR)/linux-headers-merged-x86_64-$(LINUX_HEADERS_REV).tar.gz
LINUX_HEADERS_ARM64_MERGED_FILE := $(LINUX_HEADER_BUILD_DIR)/linux-headers-merged-arm64-$(LINUX_HEADERS_REV).tar.gz
LINUX_HEADERS_GS_PATH := gs://pixie-dev-public/linux-headers/$(LINUX_HEADERS_REV)

## NATS image parameters.
NATS_IMAGE_VERSION := 2.9.19
nats_image_tag := "ghcr.io/pixie-io/nats:$(NATS_IMAGE_VERSION)-scratch"

## Copybara image parameters.
COPYBARA_IMAGE_VERSION := 20210420
copybara_image_tag := "gcr.io/pixie-oss/pixie-dev-public/copybara:$(COPYBARA_IMAGE_VERSION)"

## Elasticsearch image parameters.
ELASTICSEARCH_IMAGE_VERSION := 7.6.0-patched1
elasticsearch_image_tag := "gcr.io/pixie-oss/pixie-dev-public/elasticsearch:$(ELASTICSEARCH_IMAGE_VERSION)"

## Linux kernel for qemu/BPF tests.
KERNEL_BUILD_DIR := $(BUILD_DIR)/kernel_build
# 4.19.276, 4.14.304 are the correct versions here, but there is a bug with patch > 255.
KERNEL_BUILD_VERSIONS := 4.14.254 \
	4.19.254 \
	5.4.235 \
	5.10.173 \
	5.15.101 \
	6.1.18

KERNEL_BUILD_TEMPLATE := linux-build-%.tar.gz
KERNEL_BUILD_TARGETS = $(addprefix $(KERNEL_BUILD_DIR)/, $(patsubst %,$(KERNEL_BUILD_TEMPLATE), $(KERNEL_BUILD_VERSIONS)))
KERNEL_BUILD_TS := $(shell date +%Y%m%d%H%M%S)
KERNEL_BUILD_GS_PATH := gs://pixie-dev-public/kernel-build/$(KERNEL_BUILD_TS)

ETCD_IMAGE_VERSION := 3.5.9
ETCD_IMAGE_TAG := "gcr.io/pixie-oss/pixie-dev-public/etcd:$(ETCD_IMAGE_VERSION)"

##############################################
# Clang Build
##############################################
.PHONY: build_clang_deb_image
build_clang_deb_image:
	$(DOCKER) build clang_deb_image -t $(clang_deb_image_tag)

.PHONY: upload_clang_deb
upload_clang_deb: build_clang_deb_image ## Target to build and upload clang deb image
	@mkdir -p $(CLANG_BUILD_DIR)
	$(DOCKER) run --rm -e CLANG_SUFFIX=$(CLANG_SUFFIX) -e CLANG_VERSION=$(CLANG_VERSION) -v $(PWD)/$(CLANG_BUILD_DIR):/image $(clang_deb_image_tag)

	sha256sum $(CLANG_BUILD_DIR)/* > $(CLANG_BUILD_DIR)/sha256sums
	gsutil cp $(CLANG_BUILD_DIR)/* $(clang_gs_path)/
	$(GH_RELEASE_UPLOAD) clang $(CLANG_TAG) $(CLANG_BUILD_DIR)/*

	cat $(CLANG_BUILD_DIR)/sha256sums


##############################################
# GPerftools build
##############################################
.PHONY: build_gperftools_deb_image
build_gperftools_deb_image:
	$(DOCKER) build --build-arg BASE_IMAGE=$(DEV_EXTRAS_IMAGE) --build-arg BASE_IMAGE_DIGEST=$(DEV_EXTRAS_DIGEST) gperftools_deb_image -t $(gperftools_deb_image_tag)

.PHONY: upload_gperftools_deb
upload_gperftools_deb: build_gperftools_deb_image ## Target to build and upload gperftools deb image
	mkdir -p $(BUILD_DIR)
	$(DOCKER) run --rm -e DEB_NAME=$(gperftools_deb_fname) -e DEB_VERSION=$(GPERFTOOLS_DEB_IMAGE_VERSION) -v $(PWD)/$(BUILD_DIR):/image $(gperftools_deb_image_tag)
	gsutil cp $(PWD)/$(BUILD_DIR)/$(gperftools_deb_fname) $(gperftools_deb_gs_path)
	$(GH_RELEASE_UPLOAD) gperftools $(GPERFTOOLS_DEB_IMAGE_VERSION) $(PWD)/$(BUILD_DIR)/$(gperftools_deb_fname)
	@echo "SHA: "
	sha256sum $(PWD)/$(BUILD_DIR)/$(gperftools_deb_fname)


##############################################
# Graalvm build
##############################################
.PHONY: build_graalvm_archive_image
build_graalvm_archive_image:
	$(DOCKER) build graalvm_archive_image -t $(GRAALVM_ARCHIVE_IMAGE_TAG)

.PHONY: upload_graalvm_archive
upload_graalvm_archive: build_graalvm_archive_image
	mkdir -p $(BUILD_DIR)
	$(DOCKER) run --rm -v $(PWD)/$(BUILD_DIR):/archive $(GRAALVM_ARCHIVE_IMAGE_TAG)
	gsutil cp $(PWD)/$(BUILD_DIR)/$(GRAALVM_ARCHIVE_FNAME) $(GRAALVM_ARCHIVE_GS_PATH)
	$(GH_RELEASE_UPLOAD) graalvm $(GRAALVM_IMAGE_VERSION) $(PWD)/$(BUILD_DIR)/$(GRAALVM_ARCHIVE_FNAME)
	@echo "SHA: "
	sha256sum $(PWD)/$(BUILD_DIR)/$(GRAALVM_ARCHIVE_FNAME)


##############################################
# Linux Headers Build
##############################################
$(LINUX_HEADER_ASSETS_BUILD_DIR)/linux-headers-x86_64-%.tar.gz: linux_headers_image/Dockerfile linux_headers_image/x86_64_config
ifndef LINUX_HEADERS_REV
	$(error set LINUX_HEADERS_REV to a valid value)
endif
	@mkdir -p $(@D)
	linux_headers_image_tag="gcr.io/pixie-oss/pixie-dev-public:$*-$(LINUX_HEADERS_REV)"; \
	$(DOCKER) build --build-arg KERN_VERSION=$* --build-arg ARCH=x86_64 linux_headers_image -t "$${linux_headers_image_tag}" && \
	$(DOCKER) run --rm -v $(PWD)/$(LINUX_HEADER_ASSETS_BUILD_DIR):/output "$${linux_headers_image_tag}"

$(LINUX_HEADER_ASSETS_BUILD_DIR)/linux-headers-arm64-%.tar.gz: linux_headers_image/Dockerfile linux_headers_image/arm64_config
ifndef LINUX_HEADERS_REV
	$(error set LINUX_HEADERS_REV to a valid value)
endif
	@mkdir -p $(@D)
	linux_headers_image_tag="gcr.io/pixie-oss/pixie-dev-public:$*-$(LINUX_HEADERS_REV)"; \
	$(DOCKER) build --build-arg KERN_VERSION=$* \
		--build-arg ARCH=arm64 \
		--build-arg CROSS_COMPILE=aarch64-linux-gnu- \
		linux_headers_image -t "$${linux_headers_image_tag}" && \
	$(DOCKER) run --rm -v $(PWD)/$(LINUX_HEADER_ASSETS_BUILD_DIR):/output "$${linux_headers_image_tag}"

$(LINUX_HEADERS_X86_64_MERGED_FILE): $(LINUX_HEADER_X86_64_TARGETS)
	tar -czf $@ -C $(LINUX_HEADER_ASSETS_BUILD_DIR) $(^F)

$(LINUX_HEADERS_ARM64_MERGED_FILE): $(LINUX_HEADER_ARM64_TARGETS)
	tar -czf $@ -C $(LINUX_HEADER_ASSETS_BUILD_DIR) $(^F)

.PHONY: upload_linux_headers
upload_linux_headers:  $(LINUX_HEADERS_X86_64_MERGED_FILE) $(LINUX_HEADERS_ARM64_MERGED_FILE) ## Target to build and upload linux headers image
	gsutil cp $^ $(LINUX_HEADERS_GS_PATH)
	$(GH_RELEASE_UPLOAD) linux-headers $(LINUX_HEADERS_REV) $^
	sha256sum $^

##############################################
# Sysroots Build
##############################################
.PHONY: build_all_sysroots
build_all_sysroots: sysroot_creator/Dockerfile sysroot_creator/build_all_sysroots.sh
	@mkdir -p $(SYSROOT_BUILD_DIR)
	$(DOCKER) build sysroot_creator -t $(SYSROOT_CREATOR_IMAGE_TAG)
	./sysroot_creator/build_all_sysroots.sh $(SYSROOT_BUILD_DIR) $(SYSROOT_CREATOR_IMAGE_TAG)

.PHONY: upload_all_sysroots
upload_all_sysroots: build_all_sysroots
	gsutil cp $(SYSROOT_BUILD_DIR)/* $(SYSROOT_GS_PATH)
	$(GH_RELEASE_UPLOAD) sysroots $(SYSROOT_REV) $(SYSROOT_BUILD_DIR)/*
	sha256sum $(SYSROOT_BUILD_DIR)/*

##############################################
# Custom docker images.
##############################################
.PHONY: build_and_upload_nats_image
build_and_upload_nats_image:
	$(DOCKER) buildx build nats_image \
		--platform linux/amd64,linux/arm64 \
		--build-arg=NATS_VERSION="v$(NATS_IMAGE_VERSION)" \
		-t $(nats_image_tag) \
		--push

.PHONY: build_and_upload_copybara_image
build_and_upload_copybara_image:
	$(DOCKER) build copybara -t $(copybara_image_tag)
	$(DOCKER) push $(copybara_image_tag)

.PHONY: build_and_upload_elasticsearch_image
build_and_upload_elasticsearch_image:
	$(DOCKER) build elasticsearch -t $(elasticsearch_image_tag)
	$(DOCKER) push $(elasticsearch_image_tag)


##############################################
# Kernel Builder
##############################################
$(KERNEL_BUILD_DIR)/linux-build-%.tar.gz: kernel_builder/Dockerfile kernel_builder/kernel.config kernel_builder/build_kernel.sh
	@mkdir -p $(@D)
	kernel_builder_image_tag="gcr.io/pixie-oss/kernel-builder:$*"; \
	$(DOCKER) build --build-arg KERNEL_VERSION=$* kernel_builder -t "$${kernel_builder_image_tag}" && \
	$(DOCKER) run --rm -v $(PWD)/$(KERNEL_BUILD_DIR):/output "$${kernel_builder_image_tag}"

.PHONY: upload_kernel_build
upload_kernel_build:  $(KERNEL_BUILD_TARGETS)
	gsutil cp $^ $(KERNEL_BUILD_GS_PATH)
	$(GH_RELEASE_UPLOAD) kernel-build $(KERNEL_BUILD_TS) $^
	@echo "Add this to the qemu bzl file:"
	@echo "============================="
	@echo "kernel_build_date = ${KERNEL_BUILD_TS}"
	@echo "kernel_catalog = {"
	@sha256sum $^ | awk 'match($$0, /.*linux\-build\-(.*).tar.gz/, arr) { printf("    \"%s\": \"%s\",\n", arr[1], $$1)}'
	@echo "}"
	@echo "============================="

##############################################
# ETCD Image Builder
##############################################

upload_etcd_image:
	$(DOCKER) buildx build --platform linux/amd64,linux/arm64 -t $(ETCD_IMAGE_TAG) --push etcd_image

clean:
	@rm -rf $(BUILD_DIR)

help: ## Print help for targets with comments.
	@echo "Usage:"
	@echo "  make [target...] [VAR=foo VAR2=bar...]"
	@echo "  Do make base first, edit Dockerfile for dev image."
	@echo "  Then run make dev"
	@echo ""
	@echo "Useful commands:"
# Grab the comment prefixed with "##" after every rule.
	@grep -Eh '^[a-zA-Z._-]+:.*?## .*$$' $(MAKEFILE_LIST) |\
		sort | awk 'BEGIN {FS = ":.*?## "}; {printf "  $(cyan)%-30s$(term-reset) %s\n", $$1, $$2}'
	@echo ""
	@echo "Useful variables:"
# Grab the comment prefixed with "##" before every variable.
	@awk 'BEGIN { FS = ":=" } /^## /{x = substr($$0, 4); \
    getline; if (NF >= 2) printf "  $(cyan)%-30s$(term-reset) %s\n", $$1, x}' $(MAKEFILE_LIST) | sort
	@echo ""
	@echo "Typical usage:"
	@printf "  $(cyan)%s$(term-reset)\n    %s\n\n" \
		"make base" "Build and push the base images." \
