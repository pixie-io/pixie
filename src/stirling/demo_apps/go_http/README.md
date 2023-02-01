# HTTP server

To run, execute the following commands in two separate terminals:

`bazel run //demos/client_server_apps/go_http/server:simple_http_server`

`curl localhost:9090/ping`

Alternatively, one can use wrk_sweeper to make requests to the server.

# Push to k8s
```
# Build and deploy
./deploy.sh
# Run load test
IP=$(kubectl get service -l name=simple-service -o json | jq -r '.items[0].status.loadBalancer.ingress[0].ip')
wrk -c 2 -d 10m "http://${IP}/bm?latency=50" --latency
```
