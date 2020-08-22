## Go Dynamic Tracepoint example.
This is an example of using [gobpf](https://github.com/iovisor/gobpf) to trace arguments of the function for our example [application](https://github.com/pixie-labs/pixie/blob/main/demos/simple-gotracing/app.go).

### Dependencies
This requires [libbcc](https://github.com/iovisor/bcc/blob/master/INSTALL.md) to be installed.

### Build
```
go build trace.go
```

### Run
```
./trace --binary ./app  # To trace the example app.
```
