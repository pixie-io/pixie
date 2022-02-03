To deploy the http loadgen run:

```shell
kubectl create namespace px-http-loadtest
skaffold run -f src/e2e_test/http_loadtest/skaffold.yaml
```

To change between the two http load generator clients (wrk and hey) change the `k8s/kustomization.yaml`
to point to `wrk_client/` and `hey_client/` respectively.

You can control the size of the message being returned in `k8s/server/server_deployment.yaml`.
