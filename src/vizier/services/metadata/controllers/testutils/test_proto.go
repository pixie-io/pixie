/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

package testutils

// This file contains the protobufs used in vizier/services/metadata/controllers tests.

// AgentInfo

// NewAgentUUID is the UUID of the agent that doesn't yet exist.
var NewAgentUUID = "6ba7b810-9dad-11d1-80b4-00c04fd430c8"

// ExistingAgentUUID is the UUID of an agent that already exists and is healthy.
var ExistingAgentUUID = "7ba7b810-9dad-11d1-80b4-00c04fd430c8"

// PurgedAgentUUID is the UUID of an agent that already exists but was purged from ADS.
var PurgedAgentUUID = "4ba7b810-9dad-11d1-80b4-00c04fd430c8"

// UnhealthyAgentUUID is the UUID of an agent that exists but is unhealthy.
var UnhealthyAgentUUID = "8ba7b810-9dad-11d1-80b4-00c04fd430c8"

// KelvinAgentUUID is the UUID of a Kelvin agent.
var KelvinAgentUUID = "9ba7b810-9dad-11d1-80b4-00c04fd430c8"

// UnhealthyKelvinAgentUUID is the UUID of an unhealthy Kelvin agent.
var UnhealthyKelvinAgentUUID = "5ba7b810-9dad-11d1-80b4-00c04fd430c8"

// ExistingAgentInfo is the agent info for the healthy agent that already exists.
var ExistingAgentInfo = `
info {
  agent_id {
    high_bits: 0x7ba7b8109dad11d1
    low_bits: 0x80b400c04fd430c8
  }
  host_info {
    hostname: "testhost"
    host_ip: "127.0.0.1"
    pod_name: "pem-existing"
  }
  capabilities {
    collects_data: true
  }
}
create_time_ns: 0
last_heartbeat_ns: 65000000000
asid: 123
`

// PurgedAgentInfo is the agent info for an agent that exists but timed out of agent manager.
// This agent will try to reregister.
var PurgedAgentInfo = `
info {
  agent_id {
    high_bits: 0x4ba7b8109dad11d1
    low_bits: 0x80b400c04fd430c8
  }
  host_info {
    hostname: "purged"
    host_ip: "127.0.10.1"
    pod_name: "pem2-existing"
  }
  capabilities {
    collects_data: true
  }
}
create_time_ns: 65000000000
last_heartbeat_ns: 65000000300
asid: 159
`

// UnhealthyAgentInfo is the agent info for the unhealthy agent.
var UnhealthyAgentInfo = `
info {
  agent_id {
    high_bits: 0x8ba7b8109dad11d1
    low_bits: 0x80b400c04fd430c8
  }
  host_info {
    hostname: "anotherhost"
    host_ip: "127.0.0.2"
  }
  capabilities {
    collects_data: true
  }
}
create_time_ns: 0
last_heartbeat_ns: 0
asid: 456
`

// UnhealthyKelvinAgentInfo is the agent info for the unhealthy kelvin.
var UnhealthyKelvinAgentInfo = `
info {
  agent_id {
    high_bits: 0x5ba7b8109dad11d1
    low_bits: 0x80b400c04fd430c8
  }
  host_info {
    hostname: "abcd"
    host_ip: "127.0.0.3"
  }
  capabilities {
    collects_data: false
  }
}
create_time_ns: 0
last_heartbeat_ns: 0
asid: 789
`

// AgentStatus

// Agent1StatusPB is a protobuf for an agent status.
const Agent1StatusPB = `
agent {
  info {
    agent_id {
      high_bits: 0x11285cdd1de94ab1
      low_bits: 0xae6a0ba08c8c676c
    }
    host_info {
      hostname: "test_host"
      host_ip: "127.0.0.1"
    }
  }
  create_time_ns: 5
  asid: 123
}
status {
  state: 1
}
`

// Agent2StatusPB is the protobuf for another agent status.
const Agent2StatusPB = `
agent {
  info {
    agent_id {
      high_bits: 0x21285cdd1de94ab1
      low_bits: 0xae6a0ba08c8c676c
    }
    host_info {
      hostname: "another_host"
      host_ip: "127.0.0.1"
    }
  }
  create_time_ns: 0
  asid: 456
}
status {
  state: 1
}
`

// Processes

// ProcessCreated1PB is the protobuf for a created process.
var ProcessCreated1PB = `
upid {
  low: 89101
  high: 528280977975
}
start_timestamp_ns: 4
cmdline: "./bin/bash"
cid: "container_1"
`

// ProcessInfo1PB is the process info for the first created process.
var ProcessInfo1PB = `
upid {
  low: 89101
  high: 528280977975
}
start_timestamp_ns: 4
process_args: "./bin/bash"
cid: "container_1"
`

// ProcessCreated2PB is the protobuf for another created process.
var ProcessCreated2PB = `
upid {
  low: 468
  high: 528280977975
}
start_timestamp_ns: 4
cmdline: "test"
cid: "container_2"
`

// ProcessInfo2PB is the process info for the second created process.
var ProcessInfo2PB = `
upid {
  low: 468
  high: 528280977975
}
start_timestamp_ns: 4
process_args: "test"
cid: "container_2"
`

// ProcessTerminated1PB is the protobuf for a terminated process.
var ProcessTerminated1PB = `
upid {
  low: 89101
  high: 528280977975
}
stop_timestamp_ns: 6
`

// ProcessTerminated2PB is a protobuf for another terminated process.
var ProcessTerminated2PB = `
upid {
  low:  468
  high: 528280977975
}
stop_timestamp_ns: 10
`

// Process1PB is the protobuf for a process.
var Process1PB = `
name: 'p1'
upid {
  low: 89101
  high: 528280977975
}
cid: "container_1"
`

// Process2PB is the protobuf for another process.
var Process2PB = `
name: 'p2'
upid {
  low: 246
  high: 528280977975
}
cid: "container_2"
`

// Containers

// ContainerInfoPB is the protobuf for a container info.
var ContainerInfoPB = `
name: "container_1"
uid: "container1"
pod_uid: "ijkl"
namespace: "ns"
`

// Schema

// SchemaInfoPB is the protobuf for a schema info.
var SchemaInfoPB = `
name: "a_table"
desc: "a description"
start_timestamp_ns: 2
columns {
  name: "column_1"
  data_type: 2
}
columns {
  name: "column_2"
  data_type: 4
}
`

// SchemaInfoWithSemanticTypePB is the protobuf for a schema info with semantic type.
var SchemaInfoWithSemanticTypePB = `
name: "a_table"
desc: "a description"
start_timestamp_ns: 2
columns {
  name: "column_1"
  data_type: 2
  semantic_type: 1200
}
columns {
  name: "column_2"
  data_type: 4
}
`

// SchemaInfo2PB is the protobuf for another schema info.
var SchemaInfo2PB = `
name: "b_table"
desc: "b description"
start_timestamp_ns: 2
columns {
  name: "column_3"
  data_type: 3
}
columns {
  name: "column_4"
  data_type: 5
}
`

// SchemaInfo3PB is the protobuf for another schema info.
var SchemaInfo3PB = `
name: "c_table"
desc: "c description"
start_timestamp_ns: 2
columns {
  name: "column_5"
  data_type: 1
}
columns {
  name: "column_6"
  data_type: 2
}
`

// RegisterAgentRequest

// RegisterAgentRequestPB is the protobuf for a register agent request.
var RegisterAgentRequestPB = `
register_agent_request {
  info {
    agent_id {
      high_bits: 0x6ba7b8109dad11d1
      low_bits: 0x80b400c04fd430c8
    }
    host_info {
      hostname: "test-host"
      host_ip: "127.0.0.1"
    }
  }
}
`

// ReregisterPurgedAgentRequestPB is the protobuf for a reregister agent request.
var ReregisterPurgedAgentRequestPB = `
register_agent_request {
  info {
    agent_id {
      high_bits: 0x4ba7b8109dad11d1
      low_bits: 0x80b400c04fd430c8
    }
    host_info {
      hostname: "purged"
      host_ip: "127.0.10.1"
    }
  }
  asid: 159
}
`

// RegisterKelvinRequestPB is the protobuf for a register agent request.
var RegisterKelvinRequestPB = `
register_agent_request {
  info {
    agent_id {
      high_bits: 0x9ba7b8109dad11d1
      low_bits: 0x80b400c04fd430c8
    }
    host_info {
      hostname: "test-host"
      host_ip: "127.0.0.1"
    }
    capabilities {
      collects_data: false
    }
  }
}
`

// InvalidRegisterAgentRequestPB is an invalid protobuf of a register agent request.
var InvalidRegisterAgentRequestPB = `
register_agent_request {
  info {
    agent_id {
    }
    host_info {
      hostname: "test-host"
      host_ip: "127.0.0.1"
    }
  }
}
`

// HeartbeatAck

// HeartbeatAckPB is a protobuf for a heartbeat ack.
var HeartbeatAckPB = `
heartbeat_ack {
  update_info {
    service_cidr: "10.64.4.0/22"
    pod_cidrs: "10.64.4.0/21"
  }
}
`

// Heartbeat

// HeartbeatPB is the protobuf for a heartbeat.
var HeartbeatPB = `
heartbeat {
  time: 1,
  agent_id: {
    high_bits: 0x5ba7b8109dad11d1
    low_bits: 0x80b400c04fd430c8
  }
  update_info {
    process_created {
      cid: "test"
    }
  }
}
`

// Endpoint

// EndpointsPb is the protobuf for an endpoints object.
const EndpointsPb = `
subsets {
  addresses {
    ip: "127.0.0.1"
    hostname: "host"
    node_name: "this-is-a-node"
    target_ref {
      kind: "Pod"
      namespace: "pl"
      uid: "abcd"
      name: "pod-name"
    }
  }
  addresses {
    ip: "127.0.0.2"
    hostname: "host-2"
    node_name: "node-a"
    target_ref {
      kind: "Pod"
      namespace: "pl"
      uid: "efgh"
      name: "another-pod"
    }
  }
  not_ready_addresses {
    ip: "127.0.0.3"
    hostname: "host-3"
    node_name: "node-b"
  }
  ports {
    name: "endpt"
    port: 10,
    protocol: 1
  }
  ports {
    name: "abcd"
    port: 500,
    protocol: 1
  }
}
metadata {
  name: "object_md"
  namespace: "a_namespace"
  uid: "ijkl"
  resource_version: "1"
  creation_timestamp_ns: 4
  deletion_timestamp_ns: 6
  owner_references {
    kind: "pod"
    name: "test"
    uid: "abcd"
  }
}
`

// Service

// ServicePb is the protobuf for a service object.
const ServicePb = `
metadata {
  name: "object_md"
  namespace: "a_namespace"
  uid: "ijkl"
  resource_version: "1",
  owner_references {
    kind: "pod"
    name: "test"
    uid: "abcd"
  }
  creation_timestamp_ns: 4
  deletion_timestamp_ns: 6
}
spec {
  cluster_ip: "127.0.0.1"
  external_ips: "127.0.0.2"
  external_ips: "127.0.0.3"
  load_balancer_ip: "127.0.0.4"
  external_name: "hello"
  external_traffic_policy: 1
  ports {
    name: "endpt"
    port: 10
    protocol: 1
    node_port: 20
  }
  ports {
    name: "another_port"
    port: 50
    protocol: 1
    node_port: 60
  }
  type: 1
}
`

// Namespace

// NamespacePb is the protobuf for a generic namespace object.
const NamespacePb = `
metadata {
  name: "a_namespace"
  namespace: "a_namespace"
  uid: "ijkl"
  resource_version: "1",
  creation_timestamp_ns: 4
  deletion_timestamp_ns: 6
}
`

// Node

// NodePb is the protobuf for a generic node object.
const NodePb = `
metadata {
  name: "a_node"
  uid: "ijkl"
  resource_version: "1",
  creation_timestamp_ns: 4
  deletion_timestamp_ns: 6
}
`

// Pod

// PodPb is the protobuf for a pod object.
const PodPb = `
metadata {
  name: "object_md"
  uid: "ijkl"
  resource_version: "1",
  owner_references {
    kind: "pod"
    name: "test"
    uid: "abcd"
  }
  creation_timestamp_ns: 4
  deletion_timestamp_ns: 6
}
status {
  message: "this is message"
  phase: 2
  conditions {
    type: 2
    status: 1
  }
}
spec {
  node_name: "test"
  hostname: "hostname"
  dns_policy: 2
}
`

// PodPbWithContainers is a protobuf for a pod object that has containers.
const PodPbWithContainers = `
metadata {
  name: "object_md"
  uid: "ijkl"
  resource_version: "1",
  owner_references {
    kind: "pod"
    name: "test"
    uid: "abcd"
  }
  labels: <key:"app" value:"myApp1"> labels: <key:"project" value:"myProj1">
  creation_timestamp_ns: 4
  deletion_timestamp_ns: 6
}
status {
  message: "this is message"
  reason: "this is reason"
  phase: RUNNING
  conditions {
    type: 2
    status: 1
  }
  container_statuses {
    name: "container1"
    container_id: "docker://test"
    container_state: CONTAINER_STATE_WAITING
    message: "container state message"
    reason: "container state reason"
  }
  qos_class: QOS_CLASS_BURSTABLE
  host_ip: "127.0.0.5"
}
spec {
  node_name: "test"
  hostname: "hostname"
  dns_policy: 2
}
`

// PendingPodPb is a protobuf for a pending pod.
const PendingPodPb = `
metadata {
  name: "object_md"
  uid: "ijkl"
  resource_version: "1",
  owner_references {
    kind: "pod"
    name: "test"
    uid: "abcd"
  }
  creation_timestamp_ns: 4
  deletion_timestamp_ns: 6
}
status {
  message: "this is message"
  reason: "this is reason"
  phase: 1
  conditions {
    type: 2
    status: 2
  }
  container_statuses {
    name: "container1"
    container_state: 0
    container_id: ""
  }
  container_statuses {
    name: "container2"
    container_state: 0
    container_id: ""
  }
  qos_class: QOS_CLASS_BURSTABLE
}
spec {
  node_name: "test"
  hostname: "hostname"
  dns_policy: 2
}
`

// TerminatedPodPb is a protobuf for a terminated pod.
const TerminatedPodPb = `
metadata {
  name: "object_md"
  uid: "ijkl"
  resource_version: "1",
  owner_references {
    kind: "pod"
    name: "test"
    uid: "abcd"
  }
  creation_timestamp_ns: 4
  deletion_timestamp_ns: 6
}
status {
  message: "this is message"
  reason: "this is reason"
  phase: 5
  conditions {
    type: 2
    status: 2
  }
  container_statuses {
    name: "container1"
    container_state: 0
    container_id: ""
  }
  container_statuses {
    name: "container2"
    container_state: 0
    container_id: ""
  }
  qos_class: QOS_CLASS_BURSTABLE
}
spec {
  node_name: "test"
  hostname: "hostname"
  dns_policy: 2
}
`

// ReplicaSetPb is a protobuf for a replicaset object
const ReplicaSetPb = `
metadata {
	name: "rs_1"
	namespace: "a_namespace"
	uid: "12345"
	resource_version: "1"
	creation_timestamp_ns: 4
	deletion_timestamp_ns: 6
	labels {
		key: "env"
		value: "prod"
	}
	labels {
		key: "app"
		value: "my-test-app"
	}
	owner_references {
		kind: "deployment"
		name: "d1"
		uid: "1111"
	}
	annotations {
		key: "is_testing"
		value: "this is testing rs"
	}
	annotations {
		key: "provider"
		value: "gkee"
	}
}
spec {
	selector {
		match_expressions {
			key: "app"
			operator: "In"
			values: "hello"
			values: "world"
		}
		match_expressions {
			key: "service"
			operator: "Exists"
		}
		match_labels {
			key: "env"
			value: "prod"
		}
		match_labels {
			key: "managed"
			value: "helm"
		}
	}
	template {
		metadata {
			name: "object_md"
			namespace: "a_namespace"
			uid: "ijkl"
			resource_version: "1",
			owner_references {
			  kind: "pod"
			  name: "test"
			  uid: "abcd"
			}
			creation_timestamp_ns: 4
		}
		spec {
			node_name: "test"
			hostname: "hostname"
			dns_policy: 2
		}
	}
	replicas: 3
	min_ready_seconds: 10
}
status {
	replicas: 2
	fully_labeled_replicas: 2
	ready_replicas: 1
	available_replicas: 1
	observed_generation: 10
	conditions: {
		type: "1"
		status: 2
	}
	conditions: {
		type: "2"
		status: 1
	}
}
`

// DeploymentPb is a protobuf for a Deployment object
const DeploymentPb = `
metadata {
	name: "deployment_1"
	namespace: "a_namespace"
	uid: "ijkl"
	resource_version: "1"
	creation_timestamp_ns: 4
	deletion_timestamp_ns: 6
	owner_references {
		kind: "Pod"
		name: "pod"
		uid: "1234"
	}
	labels {
		key: "env"
		value: "prod"
	}
	labels {
		key: "app"
		value: "my-test-app"
	}
	annotations {
		key: "is_testing"
		value: "this is testing deployment"
	}
	annotations {
		key: "provider"
		value: "gkee"
	}
}
spec {
	selector {
		match_expressions {
			key: "app"
			operator: "In"
			values: "hello"
			values: "world"
		}
		match_expressions {
			key: "service"
			operator: "Exists"
		}
		match_labels {
			key: "env"
			value: "prod"
		}
		match_labels {
			key: "managed"
			value: "helm"
		}
	}
	template {
		metadata {
			name: "object_md"
			namespace: "a_namespace"
			uid: "ijkl"
			resource_version: "1",
			owner_references {
				kind: "ReplicaSet"
				name: "pod1"
				uid: "abcd"
			}
			creation_timestamp_ns: 4
		}
		spec {
			node_name: "test"
			hostname: "hostname"
			dns_policy: 2
		}
	}
	replicas: 3
	strategy {
		type: 2
		rolling_update: {
			max_unavailable: "10"
			max_surge: "5"
		}
	}
}
status {
	observed_generation: 2
	replicas: 4
  updated_replicas: 3
	ready_replicas: 2
	available_replicas: 3
  unavailable_replicas: 1
	conditions: {
		type: 1
		status: 1
    reason: "DeploymentAvailable"
    message: "Deployment replicas are available"
    last_update_time_ns: 4
    last_transition_time_ns: 5
	}
	conditions: {
		type: 2
		status: 1
    reason: "ReplicaUpdate"
    message: "Updated Replica"
    last_update_time_ns: 4
    last_transition_time_ns: 5
	}
}
`

// TDLabelSelectorPb is a protobuf for a TracepointDeployment object with a LabelSelector.
const TDLabelSelectorPb = `
name: "test_probe"
deployment_spec: {
  label_selector: {
    labels: {
      key:"app"
      value:"my_app"
    }
    namespace: "namespace1",
    container: "container1",
    process: "/app -80"
  }
},
programs: [
  {
    table_name: "test",
    bpftrace: {
      program: "uretprobe:\"readline\" { printf(\"cmd: %s\", str(retval));}"
    }
  }
]
`

// TDPodProcessPb is a protobuf for a TracepointDeployment object with a PodProcess.
const TDPodProcessPb = `
name: "test_probe"
deployment_spec: {
  pod_process : {
    pods : [
      "namespace1/pod1",
      "namespace1/pod2"
    ],
    container: "container1",
    process: "/app -80"
  }
},
programs: [
  {
    table_name: "test",
    bpftrace: {
      program: "uretprobe:\"readline\" { printf(\"cmd: %s\", str(retval));}"
    }
  }
]
`
