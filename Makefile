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

# make-lazy converts a recursive variable, which is evaluated every time it's
# referenced, to a lazy variable, which is evaluated only the first time it's
# used. See: http://blog.jgc.org/2016/07/lazy-gnu-make-variables.html
override make-lazy = $(eval $1 = $$(eval $1 := $(value $1))$$($1))

# Color support.
yellow = $(shell { tput setaf 3 || tput AF 3; } 2>/dev/null)
cyan = $(shell { tput setaf 6 || tput AF 6; } 2>/dev/null)
term-reset = $(shell { tput sgr0 || tput me; } 2>/dev/null)
$(call make-lazy,yellow)
$(call make-lazy,cyan)
$(call make-lazy,term-reset)

# This default target is invoked by CodeQL. Ensure that this is the first
# target in this makefile.
.PHONY: go
go: ## A simple go build that ensure that the go code compiles.
	CGO_ENABLED=0 go build ./...

.PHONY: clean
clean: ## Remove the bazel build directories.
	$(BAZEL) clean

.PHONY: pristine
pristine: ## Remove the bazel build directories and purge the build cache.
	$(BAZEL) clean --expunge

.PHONY: build
build: ## Run the full build.
	$(BAZEL) build //...

.PHONY: test
test: ## Run all the tests.
	$(BAZEL) test //... ${BAZEL_TEST_EXTRA_ARGS}

.PHONY: test-opt
test-opt: ## Run all the tests, optimized build.
	$(BAZEL) test -c opt //... ${BAZEL_TEST_EXTRA_ARGS}

.PHONY: test-asan
test-asan: ## Run all the tests, with address sanitizer.
	$(BAZEL) test --config=asan //... ${BAZEL_TEST_EXTRA_ARGS}

.PHONY: test-tsan
test-tsan: ## Run all the tests, with thread sanitizer.
	$(BAZEL) test --config=tsan //... ${BAZEL_TEST_EXTRA_ARGS}

.PHONY: go-mod-tidy
go-mod-tidy: ## Ensure that go are cleaned up.
	go mod tidy -compat=1.21

.PHONY: go-mod-ensure
go-mod-ensure: ## Ensure that go dependencies exist.
	go mod download

.PHONY: gazelle-repos
gazelle-repos: go.mod ## Run gazelle and generate build rules for new deps in go.mod, and go.sum.
	$(BAZEL) run //:gazelle -- update-repos \
		-from_file=go.mod \
		-prune \
		-to_macro=go_deps.bzl%pl_go_dependencies \
		-build_directives="gazelle:map_kind go_binary pl_go_binary @px//bazel:pl_build_system.bzl,gazelle:map_kind go_test pl_go_test @px//bazel:pl_build_system.bzl"

.PHONY: gazelle
gazelle: gazelle-repos ## Run gazelle and autofix bazel dependencies for go targets.
	$(BAZEL) run //:gazelle -- fix

.PHONY: buildifier
buildifier: ## Run bazel buildtools buildifier to format build files.
	$(BAZEL) run //:buildifier

.PHONY: go-setup
go-setup: go-mod-tidy go-mod-ensure gazelle ## Run go setup to regenrate modules/build files.

dev-env-start: ## Start K8s dev environment.
	./scripts/setup_dev_k8s.sh

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
