## Test Generation
### Quip Doc
https://pixie-labs.quip.com/ZrtAAUvzy6xq/MySQL-Test-Generator

### Tshark Capture scripts
The tshark capture scripts run a container test locally, use tshark to capture the traffic, and writes it to a JSON format that is later consumed by the test generator.
```
bazel build //src/stirling:mysql_container_bpf_test
mysql_tshark.sh
```
Output is in `src/stirling/source_connectors/socket_tracer/protocols/mysql/testing/tshark.json`

### MySQL Expected Test Output Generation
This consumes the raw traffic dump generated above, and outputs a trimmed JSON file that matches MySQL records. The JSON file describes an array of MySQL records expected to be captured, parsed, stitched together by stirling.
```
bazel run //src/stirling/source_connectors/socket_tracer/protocols/test_output_generator:test_generator \
-- src/stirling/source_connectors/socket_tracer/protocols/mysql/testing/tshark.json \
src/stirling/source_connectors/socket_tracer/protocols/mysql/testing/mysql_container_bpf_test.json
```