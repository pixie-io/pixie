# How to hook up a new protocol parser?

In [How to contribute a protocol parser in Pixie](protocol_contribution_guide.md), we walked through the overall parsing pipeline and the 5 core functions that need to be implemented. We are now ready to integrate the parser with the rest of Stirling.

## Protocol Inference
Before we can parse anything, we need to first recognize what type of traffic it is. Currently, when a connection opens, Stirling runs a rule-based model to predict what protocol the connection is. The model takes in a buffer of bytes (payload of TCP/UDP) and returns the type (request or response) of a specific protocol. Each protocol has its own inference rules, and are all implemented in `src/stirling/source_connectors/socket_tracer/bcc_bpf/protocol_inference.h`. For example, the HTTP inference rules look like the following.

```cpp
static __inline enum message_type_t infer_http_message(const char* buf, size_t count) {
// Smallest HTTP response is 17 characters:
// HTTP/1.1 200 OK\r\n
// Smallest HTTP response is 16 characters:
// GET x HTTP/1.1\r\n
if (count < 16) {
return kUnknown;
}

if (buf[0] == 'H' && buf[1] == 'T' && buf[2] == 'T' && buf[3] == 'P') {
return kResponse;
}
if (buf[0] == 'G' && buf[1] == 'E' && buf[2] == 'T') {
return kRequest;
}
if (buf[0] == 'H' && buf[1] == 'E' && buf[2] == 'A' && buf[3] == 'D') {
return kRequest;
}
if (buf[0] == 'P' && buf[1] == 'O' && buf[2] == 'S' && buf[3] == 'T') {
return kRequest;
}
if (buf[0] == 'P' && buf[1] == 'U' && buf[2] == 'T') {
return kRequest;
}
if (buf[0] == 'D' && buf[1] == 'E' && buf[2] == 'L' && buf[3] == 'E' && buf[4] == 'T' &&
buf[5] == 'E') {
return kRequest;
}
return kUnknown;
}
```

The rules are run sequentially, and the first protocol rule to return `kRequest` or `kResponse` becomes the classification result.

Caveats:
1. Rules for a new protocol may cause false positives or false negatives, which can affect the accuracy of other protocols.
2. The ordering of the rules matters.

For these reasons, we generally do the following:

1. The inference rules should be relatively simple and should most likely correspond to parts of the protocol’s wire documentation.
2. Avoid using overly generic and common patterns in the protocol as inference rules. For example, an opcode of `0x00` or `0x01` alone is not tight enough to confidently classify a protocol, because of how often they can occur. Missing some traffic in the beginning is much better than misclassifying the connection.
3. Sometimes, it’s only easy to infer a protocol on the request side or the response side, but not the other. It’s okay if we only have rules for one direction, and Stirling will figure out the other direction automatically.
4. We tend to put tighter and more robust rules, such as HTTP, in the front.

The Pixie team can help with testing the new protocol inference rule in a network traffic dataset. Please file a feature request issue on github or send a message in the Pixie slack channel.

## Adding a data table
Next, we should think about what columns the table should have. We should add a table spec for the new protocol in `src/stirling/source_connectors/socket_tracer`. See examples in `http_table.h` or `mysql_table.h`. Accordingly, in `src/stirling/source_connectors/socket_tracer/socket_trace_connectors`, overload the `AppendMessage()` function for the new protocol. This function appends a single record to the data table.

## Hook up the parsing pipeline
Search for `PROTOCOL_LIST` under `src/stirling/source_connectors/socket_tracer` for the complete list of places that need to be updated for the new protocol.
You'll need to read the comments and existing code associated with `PROTOCOL_LIST` for the exact changes needed.
For example, in `conn_tracker.cc`, you'll need to add the role for tracing into `CreateTraceRoles()`.

## Manual testing with Stirling Wrapper
With everything hooked up, we should test the implementation with `stirling_wrapper`, which only
runs stirling without the other Pixie components. See `src/stirling/binaries/stirling_wrapper.cc` for details.
- Build stirling_wrapper: `bazel build //src/stirling/binaries:stirling_wrapper`.
- Launch stirling stirling wrapper:
`sudo bazel-bin/src/stirling/binaries/stirling_wrapper --print_record_batches=<protocol> --timeout_secs=-1`
- Launch your test program, typically a pair of client & server applications.
If everything works correctly, you should see the messages of your protocol printed out on the console.

## Adding a trace bpf test
With everything hooked up, we should be able to test tracing the protocol end-to-end, by tracing an actual application. We need to find/write a simple client server application that uses the protocol. Take a look at `mysql_trace_bpf_test.cc` and others as an example. Similarly, we can create our custom `ContainerRunner` in `socket_tracer/testing/container_images.h`. The test spins up the client and server, generates some traffic, and checks the records in the table are as expected.

## Debugging the trace bpf test
The trace bpf test almost never passes on the first try. To debug it, we first need to figure out if the error happened in eBPF or user space.

Some suggestions:
1. In `src/stirling/source_connectors/socket_tracer/conn_tracker.cc`, add `SetDebugTrace(2);` to the top of `ConnTracker::CheckTracker()`. This turns on debug trace and we should be able to see all the traffic received by `ConnTracker` in the user space.
2. With `ConnTrace` turned on in step 1, check the log of the trace bpf test, and see if the ConnTrace logs everything you would expect. If all the expected traffic shows up in ConnTrace, this means that eBPF is tracing correctly and data has reached user space safely. Otherwise, I would suspect there’s a misclassification of traffic in eBPF and double-check the protocol inference rules. It can be tricky to debug issues in eBPF and don’t hesitate to ask the Pixie team for help.
3. If the traffic shows up as expected in ConnTrace, there is probably something wrong with the protocol parser.

    - Add a log statement at the top of `ParseFrame` to see if the buffer passed into it is expected. It’s a common bug that the buffer isn’t correctly aligned with the start of a frame. Make sure the correct length is stripped off `ParseFrame` at the end.

    - Add logs in `StitchFrames` to check that all the tags are correctly matched.

    - Check if any error was produced by the full body parsing module. Consider incorporating the failing cases into unit tests.
4. Cross checking the traced records with WireShark can also be very helpful.

## Advanced topics in parsing
###State
A `struct State` is useful in some protocols to retain information across time, such as MySQL and Kafka. The state can be useful in `ParseFrame` or `FindFrameBoundary` as additional signals to make the implementation more robust. For example, in Kafka, it stores a set of all the `correlation_id`s (tags) seen on the request buffer, and `FindFrameBoundary` only returns positions on the response buffer where the `correlation_id` has been seen before. The state is stored in `ConnTracker` and will remain available as long as the connection is open.

We should consider adding a state if there isn’t enough information in the current frame to detect whether it’s valid or where the frame ends, and that there’s additional information across frames that we can utilize.

## Conclusion
Congratulations on successfully adding a new protocol to Pixie! The new protocol parser will benefit many others thanks to your contribution. Make sure to test Pixie on your own applications and the Pixie Team would much appreciate any feedback or bug reports.
