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
REGSYNC := regsync

BASE_IMAGE_VERSION := $(shell date "+%Y%m%d%H%M")
BASE_IMAGE_VERSION_FILE := base_image_version.txt
LATEST_BASE_IMAGE 	    := $(shell cat $(BASE_IMAGE_VERSION_FILE))

DEV_IMAGE_VERSION := $(shell date "+%Y%m%d%H%M")

## The repository where the images are stored.
REPOSITORY := ghcr.io/pixie-io

LINTER_IMAGE_DIGEST := $$($(DOCKER) inspect --format='{{index .RepoDigests 0}}' \
											 $(REPOSITORY)/linter_image:$(DEV_IMAGE_VERSION) | \
											 cut -d':' -f2)
DEV_IMAGE_DIGEST := $$($(DOCKER) inspect --format='{{index .RepoDigests 0}}' \
											 $(REPOSITORY)/dev_image:$(DEV_IMAGE_VERSION) | \
											 cut -d':' -f2)
DEV_IMAGE_WITH_EXTRAS_DIGEST := $$($(DOCKER) inspect --format='{{index .RepoDigests 0}}' \
											 $(REPOSITORY)/dev_image_with_extras:$(DEV_IMAGE_VERSION) | \
											 cut -d':' -f2)

.PHONY: build_and_push_base_image
build_and_push_base_image:
	$(PACKER) build \
		-var "base_image_and_tag=ubuntu:22.04" \
		-var "generated_image_repository=$(REPOSITORY)/base_image" \
		-var "generated_image_tag=$(BASE_IMAGE_VERSION)" \
		packer_docker_base.json
	echo $(BASE_IMAGE_VERSION) > $(BASE_IMAGE_VERSION_FILE)

.PHONY: build_and_push_dev_images
build_and_push_dev_images:
	$(PACKER) build \
		-var "base_image_and_tag=ubuntu:22.04" \
		-var "generated_image_repository=$(REPOSITORY)/linter_image" \
		-var "generated_image_tag=$(DEV_IMAGE_VERSION)" \
		packer_docker_linters.json

	$(PACKER) build \
		-var "base_image_and_tag=$(REPOSITORY)/base_image:$(LATEST_BASE_IMAGE)" \
		-var "generated_image_repository=$(REPOSITORY)/dev_image" \
		-var "generated_image_tag=$(DEV_IMAGE_VERSION)" \
		packer_docker_dev.json

	$(PACKER) build \
		-var "base_image_and_tag=$(REPOSITORY)/dev_image:$(DEV_IMAGE_VERSION)" \
		-var "generated_image_repository=$(REPOSITORY)/dev_image_with_extras" \
		-var "generated_image_tag=$(DEV_IMAGE_VERSION)" \
		packer_docker_dev_extras.json

	echo "DOCKER_IMAGE_TAG=$(DEV_IMAGE_VERSION)" > $(DOCKER_PROPERTIES_FILE)
	echo "LINTER_IMAGE_DIGEST=$(LINTER_IMAGE_DIGEST)" >> $(DOCKER_PROPERTIES_FILE)
	echo "DEV_IMAGE_DIGEST=$(DEV_IMAGE_DIGEST)" >> $(DOCKER_PROPERTIES_FILE)
	echo "DEV_IMAGE_WITH_EXTRAS_DIGEST=$(DEV_IMAGE_WITH_EXTRAS_DIGEST)" >> $(DOCKER_PROPERTIES_FILE)
