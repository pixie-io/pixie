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

Simply run `make` in each of the sub-directories to build those underlying binaries.
You will need to run these on a Linux machine with [bcc](https://github.com/iovisor/bcc/blob/master/INSTALL.md) installed.

Docker build coming soon!
