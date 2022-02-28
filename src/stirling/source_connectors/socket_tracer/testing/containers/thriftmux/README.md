# Thriftmux image

This directory contains the build files for running a thriftmux client and server.

## About the container

This container can be run as a thriftmux server or client.

```
# Runs the server by default
$ bazel run src/stirling/source_connectors/socket_tracer/testing/containers/thriftmux:server_image

# Add TLS certs to java keystore
docker exec -it ${container_id} /usr/lib/jvm/java-11-openjdk-amd64/bin/keytool -importcert -keystore /etc/ssl/certs/java/cacerts -file /etc/ssl/ca.crt -noprompt -storepass changeit

# Run the client. This requires using 'docker run' directly to override the default entrypoint
$ docker run --entrypoint /usr/bin/java bazel/src/stirling/source_connectors/socket_tracer/testing/containers/thriftmux:server_image -cp @/app/px/src/stirling/source_connectors/socket_tracer/testing/containers/thriftmux/server_image.classpath Client
```

See `mux_container_bpf_test.cc` for use case.

