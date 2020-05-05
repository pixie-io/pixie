KUBECTL:=kubectl

EVAL_TMPL:=sed "s/{{USER}}/${USER}/g"

.PHONY: create_namespace
create_namespace: namespace.yaml.tmpl
	cat $< | $(EVAL_TMPL) | $(KUBECTL) apply -f -

.PHONY: create_image_pull_secret
create_image_pull_secret: image-pull-secret.yaml.tmpl
ifneq ($(wildcard ./image-pull-secret.json),)
	cat $< | $(EVAL_TMPL) | sed "s/{{IMAGE_PULL_SECRET}}/$(shell base64 -w 0 image-pull-secret.json)/g" | $(KUBECTL) apply -f -
endif

.PHONY: deploy_stirling_pod
deploy_stirling_pod: stirling-pod.yaml.tmpl create_namespace
	cat $< | $(EVAL_TMPL) | $(KUBECTL) apply -f -

.PHONY: deploy_stirling_daemonset
deploy_stirling_daemonset: stirling-daemonset.yaml.tmpl create_namespace
	cat $< | $(EVAL_TMPL) | $(KUBECTL) apply -f -

.PHONY: delete_namespace
delete_namespace: namespace.yaml.tmpl
	cat $< | $(EVAL_TMPL) | $(KUBECTL) delete -f -

.PHONY: delete_stirling_pod
delete_stirling_pod: stirling-pod.yaml.tmpl
	cat $< | $(EVAL_TMPL) | $(KUBECTL) delete -f -

.PHONY: delete_stirling_daemonset
delete_stirling_daemonset: stirling-daemonset.yaml.tmpl
	cat $< | $(EVAL_TMPL) | $(KUBECTL) delete -f -

.PHONY: delete_all
delete_all: delete_namespace
