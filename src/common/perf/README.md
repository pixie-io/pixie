# Performance measurement and optimization

## gperftools is statically linked

The setup described in [gperftools official document](https://gperftools.github.io/gperftools/cpuprofile.html)
is not used in our C++ executables. Specifically gperftools is statically linked.

## Conflicting with BCC kprobe

gperftools can conflict with BCC kprobe, causes kprobe attachment to fail with
[EAGAIN](https://github.com/iovisor/bcc/issues/2785). We have not figured out the root cause yet.
A speculation is that gperftools uses FTrace, which shares some underlying infrastructure with BCC.

One way is to disable the offending code: Inside `CreateProdSourceRegistry()` remove the call to
`RegisterOrDie<SocketTraceConnector>(...)`.

## How to trigger gperftools profiling?

To trigger profiling, you'll need explicitly call
`CPU::StartProfile(<file_path>)` and pair it with `CPU::StopProfile()` where you want to stop
profiling and write the profiling data to the specified file.

One thing to note is that if you terminate the process being profiled with signal, you'll need to
place `CPU::StopProfile()` in the signal handler.

The mechanism described in
[gperftools official document](https://gperftools.github.io/gperftools/cpuprofile.html),
which uses environment variable, does not work for our executables. We have not figured out the
root causes yet.

## Understanding the profiling results

You should export the call graph to a pdf file for easier understanding.
`pprof -pdf <executable path> <pprof>`. Note also that the executable must be compiled with symbols,
which can be verified by running `nm <executable path>`, which lists all symbols in the binary.

Other format of outputs do not present the call graph, which usually is difficult to understand.
