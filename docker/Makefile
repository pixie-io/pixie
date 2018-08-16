
# Commands.
DOCKER := docker

## The base image version, defaults to the datestamp.
BASE_IMAGE_VERSION := $(shell date "+%Y%m%d%H%M")

## The base dev image version, defaults to the datestamp.
DEV_IMAGE_VERSION := $(shell date "+%Y%m%d%H%M")

base_image_tag := "gcr.io/pl-dev-infra/base_image:$(BASE_IMAGE_VERSION)"
base_dev_image_tag := "gcr.io/pl-dev-infra/base_dev_image:$(BASE_IMAGE_VERSION)"
dev_image_tag="gcr.io/pl-dev-infra/dev_image:$(DEV_IMAGE_VERSION)"
dev_image_with_extras_tag="gcr.io/pl-dev-infra/dev_image_with_extras:$(DEV_IMAGE_VERSION)"

mkfile_path := $(abspath $(lastword $(MAKEFILE_LIST)))
mkfile_dir := $(dir $(mkfile_path))

.PHONY: build_base_image
build_base_image:
	cd $(mkfile_dir)/base_image && \
	$(DOCKER) build . -t $(base_image_tag)

.PHONY: push_base_image
push_base_image: build_base_image
	$(DOCKER) push $(base_image_tag)

.PHONY: build_base_dev_image
build_base_dev_image: build_base_image
	cd $(mkfile_dir)/base_dev_image && \
	$(DOCKER) build . -t $(base_dev_image_tag) --build-arg base_image_version=$(BASE_IMAGE_VERSION)

.PHONY: push_base_dev_image
push_base_dev_image: build_base_dev_image
	$(DOCKER) push $(base_dev_image_tag)

.PHONY: base
base: push_base_dev_image push_base_image ## Build and push the base images.
	@echo "Build/push of base images version $(BASE_IMAGE_VERSION) completed"
	@echo "At this point you should update the dev images if necessary and run \"make dev\""

.PHONY: build_dev_image
build_dev_image:
	cd $(mkfile_dir)/dev_image && \
	$(DOCKER) build . -t $(dev_image_tag)

.PHONY: push_dev_image
push_dev_image: build_dev_image
	$(DOCKER) push $(dev_image_tag)

.PHONY: build_dev_image_with_extras
build_dev_image_with_extras: build_dev_image
	cd $(mkfile_dir)/dev_image_with_extras && \
	$(DOCKER) build . -t $(dev_image_with_extras_tag) --build-arg dev_image_version=$(DEV_IMAGE_VERSION)

.PHONY: push_dev_image_with_extras
push_dev_image_with_extras: build_dev_image_with_extras
	$(DOCKER) push $(dev_image_with_extras_tag)

.PHONY: dev
dev: push_dev_image_with_extras push_dev_image ## Build and push the dev images.
	@echo "Build/push of dev images version $(DEV_IMAGE_VERSION) completed"


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
