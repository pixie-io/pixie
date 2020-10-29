## Demo of Dynamic Go Tracing

A walk through of this demo is located [here](https://docs.pixielabs.ai/tutorials/simple-go-tracing).

### Youtube Video:
[![Demo Video](https://img.youtube.com/vi/aH7PHSsiIPM/0.jpg)](https://www.youtube.com/watch?v=aH7PHSsiIPM)

### Directory structure:

- **app**: Contains the demo application we will be tracing.
- **http\_trace\_uprobe**: HTTP tracer based on uprobes on net/http.
- **http\_trace\_kprobe**: HTTP tracer based on kprobes.
- **trace\_example**: BPF example of argument tracer.

## Building the code.

1. Run `make` in each of the sub-directories to build the underlying binaries.
2. On a Linux machine with [bcc](https://github.com/iovisor/bcc/blob/master/INSTALL.md) installer, run the http server `app` binary.
3. In a new terminal window, run the kprobe with PID
```
sudo ./http_trace_kprobe --pid=<your_PID>
```
4. In a another new terminal window, use curl to make an http request
```
curl http://localhost:9090/e\?iters\=100
```
Docker build coming soon!
