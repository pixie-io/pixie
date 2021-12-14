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

## Clang deb parameters
CLANG_DEB_IMAGE_VERSION := 13.0-pl1
clang_deb_fname := "clang-$(CLANG_DEB_IMAGE_VERSION).deb"
clang_deb_gs_path :=  gs://pixie-dev-public/$(clang_deb_fname)
clang_deb_image_tag := "gcr.io/pixie-oss/pixie-dev-public/clang_deb_image:$(CLANG_DEB_IMAGE_VERSION)"

## Clang deb parameters
GPERFTOOLS_DEB_IMAGE_VERSION := 2.7-pl2
gperftools_deb_fname := "gperftools-pixie-$(GPERFTOOLS_DEB_IMAGE_VERSION).deb"
gperftools_deb_gs_path :=  gs://pixie-dev-public/$(gperftools_deb_fname)
gperftools_deb_image_tag := "gcr.io/pixie-oss/pixie-dev-public/gperftools_deb_image:$(GPERFTOOLS_DEB_IMAGE_VERSION)"

## Linux image parameters
LINUX_MAJOR_VERSION ?= 5
LINUX_KERNEL_VERSION ?= 5.3.18
LINUX_HEADERS_REV ?= pl2
linux_headers_image_tag := "gcr.io/pixie-oss/pixie-dev-public:$(LINUX_KERNEL_VERSION)-$(LINUX_HEADERS_REV)"
linux_headers_fname := linux-headers-$(LINUX_KERNEL_VERSION)-$(LINUX_HEADERS_REV).tar.gz
linux_headers_gs_path := gs://pixie-dev-public/$(linux_headers_fname)
linux_headers_trimmed_fname := linux-headers-$(LINUX_KERNEL_VERSION)-trimmed-$(LINUX_HEADERS_REV).tar.gz
linux_headers_trimmed_gs_path := gs://pixie-dev-public/$(linux_headers_trimmed_fname)

## Curl image parameters.
CURL_IMAGE_VERSION := 1.0
curl_image_tag := "gcr.io/pixie-oss/pixie-dev-public/curl:$(CURL_IMAGE_VERSION)"

## Copybara image parameters.
COPYBARA_IMAGE_VERSION := 20210420
copybara_image_tag := "gcr.io/pixie-oss/pixie-dev-public/copybara:$(COPYBARA_IMAGE_VERSION)"

## Elasticsearch image parameters.
ELASTICSEARCH_IMAGE_VERSION := 7.6.0-patched1
elasticsearch_image_tag := "gcr.io/pixie-oss/pixie-dev-public/elasticsearch:$(ELASTICSEARCH_IMAGE_VERSION)"

# Clang DEB Image configuration.
.PHONY: build_clang_deb_image
build_clang_deb_image:
	$(DOCKER) build clang_deb_image -t $(clang_deb_image_tag)

.PHONY: upload_clang_deb
upload_clang_deb: build_clang_deb_image ## Target to build and upload clang deb image
	$(DOCKER) run --rm -e deb_name=$(clang_deb_fname) -v $(PWD):/image $(clang_deb_image_tag)
	gsutil cp $(clang_deb_fname) $(clang_deb_gs_path)
	@echo "SHA: "
	sha256sum $(clang_deb_fname)
	rm -f $(clang_deb_fname)

.PHONY: build_gperftools_deb_image
build_gperftools_deb_image:
	$(DOCKER) build gperftools_deb_image -t $(gperftools_deb_image_tag)

.PHONY: upload_gperftools_deb
upload_gperftools_deb: build_gperftools_deb_image ## Target to build and upload gperftools deb image
	$(DOCKER) run --rm -e deb_name=$(gperftools_deb_fname) -v $(PWD):/image $(gperftools_deb_image_tag)
	gsutil cp $(gperftools_deb_fname) $(gperftools_deb_gs_path)
	@echo "SHA: "
	sha256sum $(gperftools_deb_fname)
	rm -f $(gperftools_deb_fname)

.PHONY: build_linux_headers_image
build_linux_headers_image:
	$(DOCKER) build --build-arg KERN_VERSION=${LINUX_KERNEL_VERSION} --build-arg KERN_MAJ=${LINUX_MAJOR_VERSION} linux_headers_image -t $(linux_headers_image_tag)

.PHONY: upload_linux_headers
upload_linux_headers: build_linux_headers_image ## Target to build and upload linux headers image
	# Extract headers from build containers.
	scripts/docker-extract.sh $(linux_headers_image_tag) /pl/linux-headers-$(LINUX_KERNEL_VERSION).tar.gz $(linux_headers_fname)
	scripts/docker-extract.sh $(linux_headers_image_tag) /pl/linux-headers-$(LINUX_KERNEL_VERSION)-trimmed.tar.gz $(linux_headers_trimmed_fname)
	# Print SHAs for convenience.
	@echo "SHA: "
	sha256sum $(linux_headers_fname)
	@echo "SHA: "
	sha256sum $(linux_headers_trimmed_fname)
	# Upload.
	gsutil cp $(linux_headers_fname) $(linux_headers_gs_path)
	gsutil cp $(linux_headers_trimmed_fname) $(linux_headers_trimmed_gs_path)
	# Cleanup.
	rm -f $(linux_headers_fname)
	rm -f $(linux_headers_trimmed_fname)

.PHONY: build_and_upload_curl_image
build_and_upload_curl_image:
	$(DOCKER) build curl_image -t $(curl_image_tag)
	$(DOCKER) push $(curl_image_tag)

.PHONY: build_and_upload_copybara_image
build_and_upload_copybara_image:
	$(DOCKER) build copybara -t $(copybara_image_tag)
	$(DOCKER) push $(copybara_image_tag)

.PHONY: build_and_upload_elasticsearch_image
build_and_upload_elasticsearch_image:
	$(DOCKER) build elasticsearch -t $(elasticsearch_image_tag)
	$(DOCKER) push $(elasticsearch_image_tag)

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
