# Makefile for the repo.
# This is used to help coordinate some of the tasks around the build process.
# Most of the actual build steps should be managed by Bazel.

## Bazel command to use.
BAZEL     := bazel

## Dep command to use.
DEP       := dep

## Minikube command to use.
MINIKUBE  := minikube

## Kubectl command to use.
KUBECTL := kubectl
KUBECTL_FLAGS := -n pl

WORKSPACE := $$(bazel info workspace)

## Skaffold command to use.
SKAFFOLD := skaffold

SKAFFOLD_DIR := $(WORKSPACE)/skaffold

## Active operating system (Linux vs MacOS).
UNAME_S := $(shell uname -s)

# Minikube flags to select vm-driver under MacOS
ifeq ($(UNAME_S),Darwin)
    MINIKUBE_START_FLAGS += --vm-driver hyperkit
endif


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
	$(BAZEL) test //... ${BAZEL_TEST_EXTRA_ARGS}

.PHONY: test-opt
test-opt: ## Run all the tests (except UI), optimized build.
	$(BAZEL) test -c opt //... ${BAZEL_TEST_EXTRA_ARGS}

.PHONY: test-asan
test-asan: ## Run all the tests (except UI), with address sanitizer.
	$(BAZEL) test --config=asan //... ${BAZEL_TEST_EXTRA_ARGS}

.PHONY: test-tsan
test-tsan: ## Run all the tests (except UI),  with thread sanitizer.
	$(BAZEL) test --config=tsan //... ${BAZEL_TEST_EXTRA_ARGS}

.PHONY: dep-ensure
dep-ensure: ## Ensure that go dependencies exist.
	$(DEP) ensure

gazelle-repos: Gopkg.lock
	$(BAZEL) run //:gazelle -- update-repos -from_file=Gopkg.lock

gazelle: gazelle-repos ## Run gazelle to update go build rules.
	$(BAZEL) run //:gazelle

go-setup: dep-ensure gazelle

k8s-load-certs:
	-$(KUBECTL) $(KUBECTL_FLAGS) delete secret proxy-tls-certs
	-$(KUBECTL) $(KUBECTL_FLAGS) delete secret service-tls-certs
	-$(KUBECTL) $(KUBECTL_FLAGS) delete secret etcd-peer-tls-certs
	-$(KUBECTL) $(KUBECTL_FLAGS) delete secret etcd-client-tls-certs
	-$(KUBECTL) $(KUBECTL_FLAGS) delete secret etcd-server-tls-certs

	$(KUBECTL) $(KUBECTL_FLAGS) create secret tls proxy-tls-certs \
		--key src/services/certs/server.key \
		--cert src/services/certs/server.crt

	$(KUBECTL) $(KUBECTL_FLAGS) create secret generic service-tls-certs \
		--from-file=server.key=src/services/certs/server.key \
		--from-file=server.crt=src/services/certs/server.crt \
		--from-file=ca.crt=src/services/certs/ca.crt \
		--from-file=client.key=src/services/certs/client.key \
		--from-file=client.crt=src/services/certs/client.crt

	$(KUBECTL) $(KUBECTL_FLAGS) create secret generic etcd-peer-tls-certs \
		--from-file=peer.key=src/services/certs/server.key \
		--from-file=peer.crt=src/services/certs/server.crt \
		--from-file=peer-ca.crt=src/services/certs/ca.crt

	$(KUBECTL) $(KUBECTL_FLAGS) create secret generic etcd-client-tls-certs \
		--from-file=etcd-client.key=src/services/certs/client.key \
		--from-file=etcd-client.crt=src/services/certs/client.crt \
		--from-file=etcd-client-ca.crt=src/services/certs/ca.crt

	$(KUBECTL) $(KUBECTL_FLAGS) create secret generic etcd-server-tls-certs \
		--from-file=server.key=src/services/certs/server.key \
		--from-file=server.crt=src/services/certs/server.crt \
		--from-file=server-ca.crt=src/services/certs/ca.crt

k8s-load-dev-secrets: #Loads the secrets used by the dev environment. At some point it might makse sense to move this into a dev setup script somewhere.
	-$(KUBECTL) $(KUBECTL_FLAGS) delete secret pl-app-secrets
	$(KUBECTL) $(KUBECTL_FLAGS) create secret generic pl-app-secrets \
		--from-literal=jwt-signing-key=ABCDEFG \
		--from-literal=session-key=test-session-key \
		--from-literal=auth0-client-id=qaAfEHQT7mRt6W0gMd9mcQwNANz9kRup \
		--from-literal=auth0-client-secret=_rY9isTWtKgx2saBXNKZmzAf1y9pnKvlm-WdmSVZOFHb9OQtWHEX4Nrh3nWE5NNt

dev-env-start: ## Start K8s dev environment.
	$(WORKSPACE)/scripts/setup_dev_k8s.sh

dev-env-stop: ## Stop dev environment.
	$(MINIKUBE) stop

dev-env-teardown: dev-env-stop ## Clean up dev environment.
	$(MINIKUBE) delete

deploy-vizier-nightly: ## Deploy vizier in nightly environment.
	PL_BUILD_TYPE=nightly $(SKAFFOLD) run -f $(SKAFFOLD_DIR)/skaffold_nightly.yaml

deploy-customer-docs-nightly: ## Deploy customer docs in nightly environment.
	PL_BUILD_TYPE=nightly $(SKAFFOLD) run -f $(SKAFFOLD_DIR)/skaffold_customer_docs.yaml

gen-jwt: ## Generate a JWT for our demo cluster.
	@JWT=$$(PL_JWT_SIGNING_KEY=ABCDEFG $(BAZEL) run //src/utils/gen_test_key); \
        echo ""; \
	echo "Paste the following into your browser console:"; \
	echo "pltoken='$$JWT';"; \
	echo "localStorage.setItem('auth', JSON.stringify({'idToken':pltoken}));"

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
