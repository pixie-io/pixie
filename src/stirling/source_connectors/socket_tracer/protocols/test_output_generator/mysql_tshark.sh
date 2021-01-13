#! /bin/bash
if [[ -z "$PIXIE_ROOT" ]]; then
    echo "Error: Need PIXIE_ROOT as an environment variable."
    exit
fi

echo "Running tshark in the background."
tshark -i any -O mysql -w "$PIXIE_ROOT"/src/stirling/source_connectors/socket_tracer/protocols/mysql/testing/raw.pcap -q > /dev/null&
TSHARK_PID=$!

echo "Running mysql_container_bpf_test."
cd "$PIXIE_ROOT" || exit
sudo ./bazel-bin/src/stirling/mysql_container_bpf_test --tracing_mode=true

echo "Dumping captured mysql traffic."
tshark -i any -2 -r "$PIXIE_ROOT"/src/stirling/source_connectors/socket_tracer/protocols/mysql/testing/raw.pcap -R mysql -T json \
> "$PIXIE_ROOT"/src/stirling/source_connectors/socket_tracer/protocols/mysql/testing/tshark.json

kill -9 "$TSHARK_PID"
