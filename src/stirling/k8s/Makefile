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

# In most cases, one should use run_stirling_wrapper_on_k8s.sh instead, which calls these targets.

KUBECTL:=kubectl

NAMESPACE ?= pl-${USER}

EVAL_TMPL:=sed -e "s/{{USER}}/${USER}/g" -e "s/{{NAMESPACE}}/${NAMESPACE}/g"

.PHONY: create_namespace
create_namespace: namespace.yaml.tmpl ## Create the target namespace for a Stirling deployment.
	cat $< | $(EVAL_TMPL) | $(KUBECTL) apply -f -

.PHONY: create_image_pull_secret
create_image_pull_secret:
	@echo "Installing image-pull-secrets (command echo suppressed)"
	@kubectl -n ${NAMESPACE} delete secret image-pull-secret 2> /dev/null || true
	@kubectl -n ${NAMESPACE} create secret docker-registry image-pull-secret --docker-server=https://gcr.io --docker-username=_json_key --docker-email=${USER}@pixielabs.ai --docker-password='$(shell sops -d ../../../../credentials/k8s/dev/image-pull-secrets.encrypted.json)'

.PHONY: deploy_stirling_pod
deploy_stirling_pod: stirling-pod.yaml.tmpl create_namespace create_image_pull_secret ## Deploy Stirling as a pod on a single node.
	cat $< | $(EVAL_TMPL) | $(KUBECTL) apply -f -

.PHONY: deploy_stirling_daemonset
deploy_stirling_daemonset: stirling-daemonset.yaml.tmpl create_namespace create_image_pull_secret  ## Deploys stirling as a daemonset on all nodes.
	cat $< | $(EVAL_TMPL) | $(KUBECTL) apply -f -

.PHONY: deploy_stirling_profiler_pod
deploy_stirling_profiler_pod: stirling-profiler-pod.yaml.tmpl create_namespace create_image_pull_secret delete_stirling_profiler_pod ## Deploy Stirling profiler as a pod on a single node.
	cat $< | $(EVAL_TMPL) | $(KUBECTL) apply -f -

.PHONY: delete_stirling_pod
delete_stirling_pod: stirling-pod.yaml.tmpl ## Delete the previously deployed Stirling pod.
	cat $< | $(EVAL_TMPL) | $(KUBECTL) delete -f -

.PHONY: delete_stirling_daemonset
delete_stirling_daemonset: stirling-daemonset.yaml.tmpl ## Delete the previously deployed Stirling daemonset.
	cat $< | $(EVAL_TMPL) | $(KUBECTL) delete -f -

.PHONY: delete_stirling_profiler_pod
delete_stirling_profiler_pod: stirling-profiler-pod.yaml.tmpl ## Delete the previously deployed Stirling pod.
	cat $< | $(EVAL_TMPL) | $(KUBECTL) delete -f -
