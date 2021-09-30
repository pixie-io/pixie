# Stirling source connectors

SourceConnector subclasses pull data from specific sources, and export them into data tables.

## Production connectors

The following source connectors are used in production deployments of Stirling/Pixie.

### SocketTracer

SocketTraceConnector reads application network traffic and exports them into the corresponding
tables for their protocols. It uses eBPF to capture application network traffic.

### JVMStats

JVMStatsConnector reads performance metrics from Java HotSpot Performance data log.

### ProcessStats

ProcessStatsConnector reports processes' CPU & memory usage metrics obtained from the Linux.

### NetworkStats

NetworkStatsConnector reports network statistics obtained from from Linux.

### PerfProfiler

PerfProfileConnector is a sampling-based profiler based on eBPF.

## Non-production connectors

Source connectors that are not registered into Stirling's runtime by default.
They are typically used as testing tools, or for demonstrative purposes.

### PIDRuntime

PIDRuntimeConnector uses eBPF to track a process' running time (excluding system suspension),
and its command line.

### ProcStat

ProcStatConnector reads the system's overall CPU usage from the `/proc` file system.

### SeqGen

SeqGenConnector generates predictable sequences of numbers and text into its output tables.
It is used in tests.

### CPUStatBPFTrace && PIDCPUUseBPFTrace

These 2 demonstrate how to implement a BPFTrace-based source connector, but are not used.
