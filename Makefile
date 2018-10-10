# Makefile for the repo.
# This is used to help coordinate some of the tasks around the build process.
# Most of the actual build steps should be managed by Bazel.

## Bazel command to use.
BAZEL     := bazel

## Dep command to use.
DEP       := dep

## Minikube command to use.
MINIKUBE  := minikube

.PHONY: clean
clean:
	$(BAZEL) clean

.PHONY: pristine
pristine:
	$(BAZEL) clean --expunge

.PHONY: build
build: ## Run the full build (except UI).
	$(BAZEL) build //...

.PHONY: test
test: ## Run all the tests (except UI).
	$(BAZEL) test //...

.PHONY: dep-ensure
dep-ensure: ## Ensure that go dependencies exist.
	$(DEP) ensure

gazelle-repos: Gopkg.lock
	$(BAZEL) run //:gazelle -- update-repos -from_file=Gopkg.lock

gazelle: gazelle-repos ## Run gazelle to update go build rules.
	$(BAZEL) run //:gazelle

go-setup: dep-ensure gazelle

dev-env-start: ## Start dev environment.
	$(MINIKUBE) start --cpus 6 --memory 8192 --mount-string="$(GOPATH)/src/pixielabs.ai/pixielabs/services/certs:/certs" --mount

dev-docker-start:
	@eval $$(minikube docker-env); ./scripts/run_docker.sh --extra_args="$(DEV_DOCKER_EXTRA_ARGS)"

dev-env-stop: ## Stop dev environment.
	$(MINIKUBE) stop

dev-env-teardown: ## Clean up dev environment.
	$(MINIKUBE) stop
	$(MINIKUBE) delete

help: ## Print help for targets with comments.
	@echo "Usage:"
	@echo "  make [target...] [VAR=foo VAR2=bar...]"
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
		"make build" "Run a clean build and update all the GO deps." \
		"make pristine" "Delete all cached builds." \
