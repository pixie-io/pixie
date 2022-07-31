# Socket tracer

Socket tracker deploys eBPF probes onto network IO syscalls (read/write, send/recv etc.),
captures data, and reassemble & parse them back into application-level protocol messages.

## Summary of important facts

* http2/gRPC tracing uses uprobes, which only capture data on K8s managed processes (through
  Metadata Service).
* OpenSSL tracing uses uprobes, which traces clear-text data trough probes on OpenSSL
  `SSL_{write,read}` functions.

## Debugging missing records for a protocol

The following is a step-by-step process for root-causing missing records for a protocol.

### Verifying raw events

The first step is to verify that the raw data events were captured by `eBPF` probes:

* First use `strace` to verify syscalls are invoked, and their arguments (i.e., the raw data) were
  as expected. (You may need to install strace with `sudo apt-get install strace` on GKE nodes):

  ```shell
  # -f is critical as it allows tracing all threads of a process.
  sudo strace -f -v -s 100 -e write -p PID 2>&1 | grep PATTERN
  ```

You should confirm that all of the expected syscalls were called, and the data matches the protocol.

If `strace` did not observe the expected data, `tshark`/`wireshark` can be used to verify network
traffic. Here the goal is to verify the network traffic matches the protocol.

* `tshark`: Use tshark to verify network traffic. `wireshark` is equivalent to tshark, but requires
   a windowing system like `X`. You can install tshark with:
  `sudo apt-get install tshark`. Or you could run it with a docker image.

  ```shell
  sudo docker run -it --rm --net container:CONTAINER_ID --privileged nicolaka/netshoot \
    tshark -f "src port 6379" -f "net IP" -Tjson -e ip -e tcp -e data
  ```

If the captured network traffic matches the expectation, then the cause of missing
protocol traffic might be that we have not traced certain syscalls used by the process.
Otherwise, the protocol traffic might be transported over non-network channels, like Unix domain
sockets.

### Verifying userspace event processing

After `strace` and `tshark`/`wireshark`, you need to verify the data events were transferred from
eBPF to userspace, and processed correctly to data records, by turning on the CONN_TRACE
debug logging for the interested process and file descriptor.

You could do this by specifying target PID and FD to `stirling_wrapper` flags:

```cpp
--stirling_conn_trace_pid=<target_pid>
--stirling_conn_trace_fd=<target_fd>
```

These flag automatically set debug trace logging level to `2`. The debug level `1` is usually for
specific events that affect the ConnTracker's state, for instance, being disabled; level `2` is for
detailed processing steps.

If `--stirling_conn_trace_fd=<target_fd>` is unspecified, all FDs of the target PID are logged.

Alternatively, specify target PID as the value of `--test_only_socket_trace_target_pid=<target_pid>`
implies `--stirling_conn_trace_pid=<target_pid>`. `--test_only_socket_trace_target_pid=<target_pid>`
also turns off event filtering inside eBPF, for example, data events of an unknown protocol would be
transferred to userspace.
