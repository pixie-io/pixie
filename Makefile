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

# Makefile for the repo.
# This is used to help coordinate some of the tasks around the build process.
# Most of the actual build steps should be managed by Bazel.

## Bazel command to use.
BAZEL     := bazel

## Minikube command to use.
MINIKUBE  := minikube

WORKSPACE := $$(bazel info workspace)

## Skaffold command to use.
SKAFFOLD := skaffold

SKAFFOLD_DIR := $(WORKSPACE)/skaffold

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

.PHONY: go-mod-tidy
go-mod-tidy: ## Ensure that go are cleaned up.
	go mod tidy

.PHONY: go-mod-ensure
go-mod-ensure: ## Ensure that go dependencies exist.
	go mod download

.PHONY: gazelle-repos
gazelle-repos: go.mod
	$(BAZEL) run //:gazelle -- update-repos -from_file=go.mod -prune -to_macro=go_deps.bzl%pl_go_dependencies

.PHONY: gazelle
gazelle: gazelle-repos
	$(BAZEL) run //:gazelle -- fix

.PHONY: buildifier
buildifier:
	$(BAZEL) run //:buildifier

.PHONY: go-setup
go-setup: go-mod-tidy go-mod-ensure gazelle ## Run go setup to regenrate modules/build files.

dev-env-start: ## Start K8s dev environment.
	$(WORKSPACE)/scripts/setup_dev_k8s.sh

dev-env-stop: ## Stop dev environment.
	$(MINIKUBE) stop

dev-env-teardown: dev-env-stop ## Clean up dev environment.
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
		"make build" "Run a clean build." \
		"make go-setup" "Update go deps by re-generating go modules and build files." \
		"make pristine" "Delete all cached builds." \
