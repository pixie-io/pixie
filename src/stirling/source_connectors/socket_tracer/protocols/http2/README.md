# HTTP2/gRPC tracing

## Introduction

Pixie traces [unary](https://grpc.io/docs/what-is-grpc/core-concepts/#service-definition) gRPC RPCs
for Golang applications. This feature also works under TLS mode.

Streaming RPCs, including client-streaming, server streaming, and bidirectional streaming, are not
supported yet. And non-Golang applications are not supported either.

## Requirements

Pixie requires the following conditions of the applications under monitoring in order to work
properly:

* The executable must embed debug symbols. To verify, `nm <executable>`, should print out debug
  symbols. An output of `no symbols` means the application has no symbol, and won't be traced.
  Default `go build` includes debug symbols. You need to remove `-s` from `go build -ldflags`.
* The executable must embed dwarf symbols. To verify, `dwarfdump <executable>` (
  or `llvm-dwarfdump <executable>`). An output of `No DWARF information present` means the
  application has no dwarf symbols, and won't be traced.
  Default `go build` includes dwarf symbols. You need to remove `-w` from `go build -ldflags`.
* The application must be using Golang's standard `net/http` and `http2` packages as gRPC transport.
  Applications using other package won't be traced.

## Overview of implementation

[gRPC uses HTTP2](https://github.com/grpc/grpc/blob/master/doc/PROTOCOL-HTTP2.md)
as the underlying transport protocol. Pixie's gRPC tracing, therefore, works by directly probing
Golang's HTTP2 APIs. Particularly, Pixie probes APIs that write HTTP2 headers and frames.

Unary gRPC RPCs uses HTTP2 stream ID to match requests and responses. Pixie uses this fact to
associate the requests and responses, and produces trace records.
