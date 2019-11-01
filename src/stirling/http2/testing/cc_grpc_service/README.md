# Sample gRPC server & client in C++

## Server reflection

You'll need to include `@com_github_grpc_grpc//:grpc++_reflection` in `deps` to enable reflection
for C++ server.

Build and run `grpc_cli` to verify that the reflection works.

```shell
git clone https://github.com/grpc/grpc.git
cd grpc
make grpc_cli
bins/opt/grpc_cli ls -l localhost:50051
```
