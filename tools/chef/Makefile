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

TOT		       := $$(git rev-parse --show-toplevel)
DOCKER_PROPERTIES_FILE := $(TOT)/docker.properties

# Commands.
DOCKER := docker
PACKER := packer

## The base image version, defaults to the datestamp.
BASE_DEV_IMAGE_VERSION := $(shell date "+%Y%m%d%H%M")

BASE_DEV_IMAGE_VERSION_FILE := base_image_version.txt
LATEST_BASE_DEV_IMAGE 	    := $(shell cat $(BASE_DEV_IMAGE_VERSION_FILE))

## The base dev image version, defaults to the datestamp.
DEV_IMAGE_VERSION := $(shell date "+%Y%m%d%H%M")

## The repository where the images are stored.
REPOSITORY := gcr.io/pixie-oss/pixie-dev-public

.PHONY: build_and_push_base_dev_image
build_and_push_base_dev_image:
	$(PACKER) build \
		-var "base_image_and_tag=ubuntu:20.04" \
		-var "generated_image_repository=$(REPOSITORY)/base_dev_image" \
		-var "generated_image_tag=$(BASE_DEV_IMAGE_VERSION)" \
		packer_docker_dev.json
	echo $(BASE_DEV_IMAGE_VERSION) > $(BASE_DEV_IMAGE_VERSION_FILE)

.PHONY: build_and_push_dev_images
build_and_push_dev_images:
	$(PACKER) build \
		-var "base_image_and_tag=$(REPOSITORY)/base_dev_image:$(LATEST_BASE_DEV_IMAGE)" \
		-var "generated_image_repository=$(REPOSITORY)/dev_image" \
		-var "generated_image_tag=$(DEV_IMAGE_VERSION)" \
		packer_docker_dev.json

	$(PACKER) build \
		-var "base_image_and_tag=$(REPOSITORY)/dev_image:$(DEV_IMAGE_VERSION)" \
		-var "generated_image_repository=$(REPOSITORY)/dev_image_with_extras" \
		-var "generated_image_tag=$(DEV_IMAGE_VERSION)" \
		packer_docker_dev_extras.json

	echo "DOCKER_IMAGE_TAG=$(DEV_IMAGE_VERSION)" > $(DOCKER_PROPERTIES_FILE)

.PHONY: build_gcp_image
build_gcp_image: ## Build the dev image for GCP.
	$(PACKER) build packer_gcp_dev_extras.json

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
