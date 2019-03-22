
# Commands.
DOCKER := docker

## SSH private key.
SSH_PRIVATE_KEY_FILE := ${HOME}/.ssh/id_rsa
SSH_PRIVATE_KEY := `cat $(SSH_PRIVATE_KEY_FILE)`

## BCC deb parameters
BCC_DEB_IMAGE_VERSION := 1.1
bcc_deb_fname := "bcc-pixie-$(BCC_DEB_IMAGE_VERSION).deb"
bcc_deb_gs_path :=  gs://pl-infra-dev-artifacts/$(bcc_deb_fname)
bcc_deb_image_tag := "gcr.io/pl-dev-infra/bcc_deb_image:$(BCC_DEB_IMAGE_VERSION)"

## Clang deb parameters
CLANG_DEB_IMAGE_VERSION := 7.0-pl8
clang_deb_fname := "clang-$(CLANG_DEB_IMAGE_VERSION).deb"
clang_deb_gs_path :=  gs://pl-infra-dev-artifacts/$(clang_deb_fname)
clang_deb_image_tag := "gcr.io/pl-dev-infra/clang_deb_image:$(CLANG_DEB_IMAGE_VERSION)"

## Clang deb parameters
GPERFTOOLS_DEB_IMAGE_VERSION := 2.7-pl1
gperftools_deb_fname := "gperftools-pixie-$(GPERFTOOLS_DEB_IMAGE_VERSION).deb"
gperftools_deb_gs_path :=  gs://pl-infra-dev-artifacts/$(gperftools_deb_fname)
gperftools_deb_image_tag := "gcr.io/pl-dev-infra/gperftools_deb_image:$(GPERFTOOLS_DEB_IMAGE_VERSION)"

mkfile_path := $(abspath $(lastword $(MAKEFILE_LIST)))
mkfile_dir := $(dir $(mkfile_path))

# BCC DEB Image configuration.
.PHONY: build_bcc_deb_image
build_bcc_deb_image:
	cd $(mkfile_dir)/bcc_deb_image && \
	$(DOCKER) build . --build-arg SSH_PRIVATE_KEY="${SSH_PRIVATE_KEY}" -t $(bcc_deb_image_tag)

.PHONY: upload_bcc_deb
upload_bcc_deb: build_bcc_deb_image  ## Target to build and upload BCC deb image
	$(DOCKER) run --rm -e deb_name=$(bcc_deb_fname) -e deb_version=$(BCC_DEB_IMAGE_VERSION) -v ${PWD}:/image $(bcc_deb_image_tag)
	gsutil cp $(bcc_deb_fname)  gs://pl-infra-dev-artifacts/$(bcc_deb_fname)
	gsutil acl ch -u allUsers:READER ${bcc_deb_gs_path}
	@echo "SHA: $(shell sha256sum ${bcc_deb_fname})"
	rm -f $(bcc_deb_fname)

# Clang DEB Image configuration.
.PHONY: build_clang_deb_image
build_clang_deb_image:
	cd $(mkfile_dir)/clang_deb_image && \
	$(DOCKER) build . -t $(clang_deb_image_tag)

.PHONY: upload_clang_deb
upload_clang_deb: build_clang_deb_image ## Target to build and upload clang deb image
	$(DOCKER) run --rm -e deb_name=$(clang_deb_fname) -v ${PWD}:/image $(clang_deb_image_tag)
	gsutil cp $(clang_deb_fname)  ${clang_deb_gs_path}
	gsutil acl ch -u allUsers:READER ${clang_deb_gs_path}
	@echo "SHA: "
	sha256sum $(clang_deb_fname)
	rm -f $(clang_deb_fname)

.PHONY: build_gperftools_deb_image
build_gperftools_deb_image:
	cd $(mkfile_dir)/gperftools_deb_image && \
	$(DOCKER) build . -t $(gperftools_deb_image_tag)

.PHONY: upload_gperftools_deb
upload_gperftools_deb: build_gperftools_deb_image ## Target to build and upload gperftools deb image
	$(DOCKER) run --rm -e deb_name=$(gperftools_deb_fname) -v ${PWD}:/image $(gperftools_deb_image_tag)
	gsutil cp $(gperftools_deb_fname)  ${gperftools_deb_gs_path}
	gsutil acl ch -u allUsers:READER ${gperftools_deb_gs_path}
	@echo "SHA: "
	sha256sum $(gperftools_deb_fname)
	rm -f $(gperftools_deb_fname)

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
