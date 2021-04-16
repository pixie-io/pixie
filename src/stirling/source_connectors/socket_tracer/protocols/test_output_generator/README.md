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

### Running Tshark on a docker container

Wireshark/tshark won't capture network traffic in a different network namespace. This includes containers.
To trace within a network namespace, tshark should be run in the network namespace of the container.

Below is an example of tracing a mysqld container via tshark
```
# Terminal 1 - Run mysql server
docker run --pid=host --env=MYSQL_ALLOW_EMPTY_PASSWORD=1 --env=MYSQL_ROOT_HOST=% --name mysql_server_749245969612081 mysql/mysql-server:8.0.13

# Terminal 2 - Run tshark
ps -ef | grep mysqld
sudo nsenter -t 3269640 -n
tshark -i any -O mysql -Y mysql

# Terminal 3 - Run mysql client
docker run --rm --pid=host --network=container:mysql_server_749245969612081 -v/home/oazizi/src/px.dev/pixie/src/stirling/source_connectors/socket_tracer/protocols/mysql/testing:/scripts --name mysql_client_749286353166542 bazel/src/stirling/testing/app_containers/mysql:mysql_connector_image /scripts/script.py
```
