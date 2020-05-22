# This Makefile has basic commands for deploying Stirling from the provided yaml templates.
# In most cases, one should use run_on_k8s.sh instead, which calls these targets.

KUBECTL:=kubectl

EVAL_TMPL:=sed "s/{{USER}}/${USER}/g"

.PHONY: create_namespace
create_namespace: namespace.yaml.tmpl ## Create the target namespace for a Stirling deployment.
	cat $< | $(EVAL_TMPL) | $(KUBECTL) apply -f -

.PHONY: create_image_pull_secret
create_image_pull_secret: image-pull-secret.yaml.tmpl  ## Install image pull secrets, required for non GKE clusters.
ifneq ($(wildcard ./image-pull-secret.json),)
	@echo "Installing image-pull-secrets (command echo suppressed)"
	@cat $< | $(EVAL_TMPL) | sed "s/{{IMAGE_PULL_SECRET}}/$(shell base64 -w 0 image-pull-secret.json)/g" | $(KUBECTL) apply -f -
endif

.PHONY: deploy_stirling_pod
deploy_stirling_pod: stirling-pod.yaml.tmpl create_namespace create_image_pull_secret ## Deploy Stirling as a pod on a single node.
	cat $< | $(EVAL_TMPL) | $(KUBECTL) apply -f -

.PHONY: deploy_stirling_daemonset
deploy_stirling_daemonset: stirling-daemonset.yaml.tmpl create_namespace create_image_pull_secret  ## Deploys stirling as a daemonset on all nodes.
	cat $< | $(EVAL_TMPL) | $(KUBECTL) apply -f -

.PHONY: delete_namespace
delete_namespace: namespace.yaml.tmpl ## Delete the namespace (along with all pods, etc.).
	cat $< | $(EVAL_TMPL) | $(KUBECTL) delete -f -

.PHONY: delete_stirling_pod
delete_stirling_pod: stirling-pod.yaml.tmpl ## Delete the previously deployed Stirling pod.
	cat $< | $(EVAL_TMPL) | $(KUBECTL) delete -f -

.PHONY: delete_stirling_daemonset
delete_stirling_daemonset: stirling-daemonset.yaml.tmpl ## Delete the previously deployed Stirling daemonset.
	cat $< | $(EVAL_TMPL) | $(KUBECTL) delete -f -

.PHONY: delete_all
delete_all: delete_namespace ## An alias for delete namespace. Deletes all previously Stirling deployments.
