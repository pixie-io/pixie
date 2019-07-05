KUBECTL:=kubectl

EVAL_TMPL:=sed "s/{{USER}}/${USER}/g"

.PHONY: create_namespace
create_namespace: namespace.yaml.tmpl
	cat $< | $(EVAL_TMPL) | $(KUBECTL) apply -f -

.PHONY: deploy_stirling_pod
deploy_stirling_pod: stirling-pod.yaml.tmpl create_namespace
	cat $< | $(EVAL_TMPL) | $(KUBECTL) apply -f -

.PHONY: deploy_stirling_daemonset create_namespace
deploy_stirling_daemonset: stirling-daemonset.yaml.tmpl
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
delete_all: delete_stirling_pod delete_stirling_daemonset delete_namespace
