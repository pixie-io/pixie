# Hello world client & server for testing gRPC C tracing for Python

Python's gRPC module is built from the official gRPC C code. In addition to implementing the BCC C
code that dereferences pointers during tracing, to trace Python gRPC app, Pixie must identify the
.so library file for the Python process.

Due to implementation limitations, the test app uses gRPC version 1.19.0, which is released in 2018.
The newer version of gRPC modules were stripped of symbols. Without symbols, Pixie cannot find the
correct addresses to attach eBPF probes.

Run `update_ghcr.sh` in this directory to push image to the ghcr.io repo.

The produced docker image's entrypoint is Python REPL environment. To run the client and server:

```
docker run --name=server --rm gcr.io/pixie-oss/pixie-dev-public/python_grpc_1_19_0_helloworld:1.0 python helloworld/greeter_server.py
docker run --network=container:server --rm gcr.io/pixie-oss/pixie-dev-public/python_grpc_1_19_0_helloworld:1.0 python helloworld/greeter_client.py
```
