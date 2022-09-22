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
import json
import argparse


def create_pod2pid():
    """
    Build a mapping of running pod names to host pids.
    """
    # This cmd gets the host pid of each running container and its corresponding labels.
    list_ps_cmd = '''kubectl node-shell ''' + node + ''' -- bash -c "docker ps -q | \
    xargs docker inspect --format '{{.State.Pid}}{{json .Config.Labels}}'"'''

    pod2pid = {}
    ps_out = subprocess.run(list_ps_cmd, shell=True, capture_output=True)
    rows = str(ps_out.stdout).split('\\n')[1:-3]
    for row in rows:
        # Example: '3217413{..., "io.kubernetes.pod.name": "vizier-query-broker", ...}'
        pid_cutoff_idx = row.index("{")
        pid = row[:pid_cutoff_idx]
        try:
            kube_info = json.loads(format(row[pid_cutoff_idx:]).replace("\\\\", "\\"))
        except RuntimeError:
            print(f"Failed to parse config for pid: {pid}. Skipping to the next pid.")
            continue
        pod_name = kube_info['io.kubernetes.pod.name']
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

    pod2pid = create_pod2pid()
    trace_pods(pod2pid, node, pods, duration)
