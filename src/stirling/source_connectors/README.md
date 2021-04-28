# Stirling source connectors

SourceConnector subclasses that pull data from specific sources, and export them into the
corresponding data tables.

## SockeTracer

SocketTraceConnector reads application network traffic and exports them into the corresponding
tables for their protocols. It is Stirling's primary data source. It uses eBPF to capture
application network traffic, and uses [BCC](https://github.com/iovisor/bcc) to manage the eBPF code.

## JVMStats

JVMStatsConnector reads performance metrics from Java HotSpot Performanance data log.

## SystemStats

SystemStatsConnector reads processes' CPU & memory usage metrics (exported to `process_stats` table), and network statistics (exported to `network_stats` table) from `/proc` file system.

## PerfProfiler

PerfProfileConnector is a sampling-based profiler based on eBPF.

## Non-prod connectors

Source connectors that are not registered into Stirling's runtime.

## PIDRuntime

PIDRuntimeConnector uses eBPF to track a process' running time (excluding system suspension),
and its command line.

## ProcStat

ProcStatConnector reads the system's overall CPU usage from the `/proc` file system.

## SeqGen

SeqGenConnector generates random integer and exports them into dummy tables. It is used in tests.

## CPUStatBPFTrace && PIDCPUUseBPFTrace

These 2 demonstrate how to implement a BPFTrace-based source connector, but are not used.
