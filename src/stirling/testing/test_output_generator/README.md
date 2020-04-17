## Test Generation
### Quip Doc
https://pixie-labs.quip.com/ZrtAAUvzy6xq/MySQL-Test-Generator

### Tshark Capture scripts
The tshark capture scripts run a container test locally, use tshark to capture the traffic, and writes it to a JSON format that is later consumed by the test generator.
```
bazel build //src/stirling:mysql_container_bpf_test
mysql_tshark.sh
```
Output is in `src/stirling/mysql/testing/tshark.json`
