# Certless agent spec

`agent_daemonset.yaml` does not have specs for installing certs. Although certs are required for
production deployment.  This setup is necessary to given a base version for the headless agent to
extend upon.

Because in Kustomize, removing elements from array or replace the entire array is rather complicated
and is generally discouraged
(https://github.com/kubernetes-sigs/kustomize/blob/master/docs/eschewedFeatures.md).
We therefore choose this approach.
