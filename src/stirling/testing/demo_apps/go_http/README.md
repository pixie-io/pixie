# HTTP client & server applications

To push container image to GCR, run `:push_image` target with `--stamp`:

```
bazel run --stamp //src/stirling/testing/demo_apps/go_http/go_http_client:push_image
bazel run --stamp //src/stirling/testing/demo_apps/go_http/go_http_server:push_image
```

To deploy the client & server onto a Kubernetes cluster:

```
sed "s/{{USER}}/$USER/" src/stirling/testing/demo_apps/go_http/go_http_{client,server}/deployment.yaml | kubectl apply -f - -n go-http
```
