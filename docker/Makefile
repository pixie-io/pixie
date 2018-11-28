
# Commands.
DOCKER := docker

## The BCC deb image version, defaults to the datestamp.
BCC_DEB_IMAGE_VERSION := 1.0

## The BCC deb name.
bcc_deb_fname := "bcc-pixie-$(BCC_DEB_IMAGE_VERSION).deb"

## SSH private key.
SSH_PRIVATE_KEY_FILE := ${HOME}/.ssh/id_rsa
SSH_PRIVATE_KEY := `cat $(SSH_PRIVATE_KEY_FILE)`

bcc_deb_image_tag := "gcr.io/pl-dev-infra/bcc_deb_image:$(BCC_DEB_IMAGE_VERSION)"

mkfile_path := $(abspath $(lastword $(MAKEFILE_LIST)))
mkfile_dir := $(dir $(mkfile_path))

# BCC DEB Image configuration.
.PHONY: build_bcc_deb_image
build_bcc_deb_image:
	cd $(mkfile_dir)/bcc_deb_image && \
	$(DOCKER) build . --build-arg SSH_PRIVATE_KEY="${SSH_PRIVATE_KEY}" -t $(bcc_deb_image_tag)

.PHONY: push_bcc_deb_image
push_bcc_deb_image: build_bcc_deb_image
	$(DOCKER) push $(bcc_deb_image_tag)

.PHONY: build_bcc_deb
build_bcc_deb:
	$(DOCKER) run --rm -e deb_name=$(bcc_deb_fname) -v ${PWD}:/image $(bcc_deb_image_tag)

.PHONY: upload_bcc_deb
upload_bcc_deb: build_bcc_deb
	gsutil cp $(bcc_deb_fname)  gs://pl-infra-dev-artifacts/$(bcc_deb_fname)

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
