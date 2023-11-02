## Protocol Inference Data Collection

The following script traces applications deployed on a Kubernetes node. It gets a
`node-shell` into the specified node, and launches a process for each of the processes matching the
pods listed in `pod_names.txt`. Each process runs `tshark` in the network namespace of the pid
for `duration` seconds. Please make sure to install `kubectl-node-shell`
(https://github.com/kvaps/kubectl-node-shell) locally and install `tshark` on the target nodes.

### Generate all running pods

```shell script
kubectl get pods -A --no-headers -o custom-columns=":metadata.name" > pod_names.txt
```

### Run data collection

```shell script
bazel run src/stirling/protocol_inference:data_collection -- --node <node_name> --pods pod_names.txt --duration 30
```

The script generates a `pcap` file for each process, and is stored on `/captures` on the specified
node.

### Run dataset generation

Put the `captures` folders into one dataset folder, like below:
```shell
Input format:
--dataset
    --Captures1
        --pod1
            --1234.pcapng
        --pod2
            --2345.pcapng
    --Captures2
        --pod1
```
Run the following command:
```shell script
bazel run src/stirling/protocol_inference:dataset_generation -- --dataset <path to dataset folder>
```

This script does the following:

1. Runs a second pass analysis with Tshark to extract payload, ports, protocol etc.
2. Breaks up tcp packets using packet lengths
3. Deduplicates identical packets
4. Writes packet-level data into `packet_dataset.tsv` and connection-level data into
   `conn_dataset.tsv`

#### packet-level dataset

One row in the packet-level dataset contains the row bytes of a single packet.
Packets are parsed out from TCP/UDP payloads based on the protocol. For example, one MySQL message
can contain multiple packets. Protocol inference on the packet level can be difficult, because many
packets are fairly small and may not contain much information.

#### connection-level dataset

One row in the connection-level dataset contains a series of packets over time in a connection,
which is defined uniquely by `src_addr`, `dst_addr`, `src_port`, and `dst_port`. This enables protocol inference on
a series of packets in a connection. The goal is to evaluate if a connection is eventually correctly
classified over a period over time.

#### bidirectional-connection-level dataset

One row in the bidirectional-connection-level dataset contains a series of packets over time in a bidrectional connection.
Packets on both directions of a connection are merged by their `src_addr`, `dst_addr`, `src_port`, and `dst_port` and grouped to
make the direction agnostic. This enables protocol inference on a series of packets in a bidirectional connection. The goal is
to evaluate if at least one side of a connection can be classified to infer the protocol of the entire bidirectional connection.

## Protocol Inference Eval

There should be three tsv files `packet_dataset.tsv`, `conn_dataset.tsv` and `bi_dir_conn_dataset.tsv` in the dataset folder.
Right now, available models are {ruleset_basic, ruleset_basic_conn}.
```shell script
bazel run src/stirling/protocol_inference:eval -- --dataset <packet_dataset.tsv> --num_workers 8
--batch 256 --output_dir <path to output dir> --model <model name>
```
