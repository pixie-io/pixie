# HTTP server

To run, execute the following commands in two separate terminals:

`bazel run //demos/client_server_apps/go_http/server:simple_http_server`

`curl localhost:9090/ping`

Alternatively, one can use wrk_sweeper to make requests to the server.

