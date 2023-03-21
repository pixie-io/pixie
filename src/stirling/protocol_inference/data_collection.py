# Copyright 2018- The Pixie Authors.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# SPDX-License-Identifier: Apache-2.0

import os
import subprocess
import argparse


def create_pod2pid(node, runtime_cli):
    """
    Builds a mapping of running pod names to host pids.
    """

    binaries = {
        'docker': 'docker',
        'crictl': 'crictl',
    }
    ps_templates = {
        'docker': '''--format   '{{.State.Pid}} {{index .Config.Labels \\"io.kubernetes.pod.name\\"}}' ''',
        'crictl': '''--template '{{.info.pid}}  {{index .info.config.labels \\"io.kubernetes.pod.name\\"}}' ''',
    }
    binary = binaries[runtime_cli]
    ps_template = ps_templates[runtime_cli]

    # This command prints one line of output per container where each line is the pid of the container, a
    # single space and the name of the k8s pod determined by the io.kubernetes.pod.name container label.
    # See the output below for example output:

    # $ sudo crictl ps -q | sudo xargs crictl inspect --template \
    #     '{{.info.pid}} {{index .info.config.labels "io.kubernetes.pod.name"}}' -o go-template
    # 3635260 vizier-pem-sx7pr
    # 3634678 kelvin-cdf78c57c-qlzlb
    cmd = (
        f"kubectl node-shell {node} -- "
        f"sudo bash -c \"{binary} ps -q | xargs {binary} inspect -o go-template {ps_template}\""
    )

    pod2pid = {}
    ps_out = subprocess.run(cmd, shell=True, capture_output=True)

    rows = ps_out.stdout.decode('utf-8').splitlines()
    for row in rows:
        split_row = row.split()

        if len(split_row) != 2:
            print(f"Failed to parse config for row: {row}. Skipping to new pid")
            continue

        pid, pod_name = split_row
        pod2pid[pod_name] = pid

    return pod2pid


def trace_pods(pod2pid, node, pods, duration):
    """
    Given a mapping from pod names to host pids and a node name, trace pods whose names are
    specified in the input file for duration seconds.
    """
    # Create folder for collecting pcap files.
    subprocess.run(f"kubectl node-shell {node} -- mkdir -p /captures", shell=True,
                   capture_output=False)

    tracing_ps = []
    for pod in pods:
        if pod not in pod2pid:
            print(f"Pod name: {pod} does not exist. Skipping to the next pod.")
            continue
        pid = pod2pid[pod]
        cmd = f"kubectl node-shell {node} -- sh -c 'mkdir -p /captures/{pod}; nsenter -t {pid} -n tshark -i any \
        -a duration:{duration} -w /captures/{pod}/{pid}.pcapng'"
        print(cmd)
        p = subprocess.Popen(cmd, shell=True)
        tracing_ps.append(p)
        print(f"Capturing pid {pid}.")

    for p in tracing_ps:
        p.wait()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Protocol Dataset Collection")
    parser.add_argument('--node', type=str, required=True)
    parser.add_argument('--container-runtime-cli',
                        type=str, default='crictl',
                        help='The cli to use to find running containers, defaults to crictl')
    parser.add_argument('--pods', '-p', type=str, default="pod_names.txt")
    parser.add_argument('--duration', type=int, default=30)
    args = parser.parse_args()

    # Check that the file with pod names exists.
    assert os.path.isfile(args.pods)
    # Load the pod names to trace.
    with open(args.pods, "r") as f:
        pods = [line.strip() for line in f.readlines()]

    node = args.node
    duration = args.duration

    pod2pid = create_pod2pid(node, args.container_runtime_cli)
    trace_pods(pod2pid, node, pods, duration)
