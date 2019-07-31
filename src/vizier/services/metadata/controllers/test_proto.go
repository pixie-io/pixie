package controllers_test

import ()

// This file contains the protobufs used in vizier/services/metadata/controllers tests.

// AgentInfo
const clockNowNS = 1E9 * 70                  // 70s in NS. This is slightly greater than the expiration time for the unhealthy agent.
const healthyAgentLastHeartbeatNS = 1E9 * 65 // 65 seconds in NS. This is slightly less than the current time.

var newAgentUUID = "6ba7b810-9dad-11d1-80b4-00c04fd430c8"
var existingAgentUUID = "7ba7b810-9dad-11d1-80b4-00c04fd430c8"
var unhealthyAgentUUID = "8ba7b810-9dad-11d1-80b4-00c04fd430c8"

// LastHeartBeatNS is 65 seconds, in NS.
var existingAgentInfo = `
agent_id {
  data: "7ba7b8109dad11d180b400c04fd430c8"
}
host_info {
  hostname: "testhost"
}
create_time_ns: 0
last_heartbeat_ns: 65000000000
`

var unhealthyAgentInfo = `
agent_id {
  data: "8ba7b8109dad11d180b400c04fd430c8"
}
host_info {
  hostname: "anotherhost"
}
create_time_ns: 0
last_heartbeat_ns: 0
`

// AgentStatus

const agent1StatusPB = `
info {
  agent_id {
    data: "11285cdd-1de9-4ab1-ae6a-0ba08c8c676c"
  }
  host_info {
    hostname: "test_host"
  }
}
last_heartbeat_ns: 60
create_time_ns: 5
state: 1
`

const agent2StatusPB = `
info {
  agent_id {
    data: "21285cdd-1de9-4ab1-ae6a-0ba08c8c676c"
  }
  host_info {
    hostname: "another_host"
  }
}
last_heartbeat_ns: 50
create_time_ns: 0
state: 1
`

// Processes

var processCreated1PB = `
upid {
  low: 89101
  high: 528280977975
}
pid: 123
start_timestamp_ns: 4
cmdline: "./bin/bash"
cid: "container_1"
`

var processInfo1PB = `
upid {
  low: 89101
  high: 528280977975
}
pid: 123
start_timestamp_ns: 4
process_args: "./bin/bash"
cid: "container_1"
`

var processCreated2PB = `
upid {
  low: 468
  high: 528280977975
}
pid: 456
start_timestamp_ns: 4
cmdline: "test"
cid: "container_2"
`

var processInfo2PB = `
upid {
  low: 468
  high: 528280977975
}
pid: 456
start_timestamp_ns: 4
process_args: "test"
cid: "container_2"
`

var processTerminated1PB = `
upid {
  low: 89101
  high: 528280977975
}
stop_timestamp_ns: 6
`

var processTerminated2PB = `
upid {
  low:  468
  high: 528280977975
}
stop_timestamp_ns: 10
`

var process1PB = `
name: 'p1'
upid {
  low: 89101
  high: 528280977975
}
cid: "container_1"
`

var process2PB = `
name: 'p2'
upid {
  low: 246
  high: 528280977975
}
cid: "container_2"
`

// Containers

var containerInfoPB = `
name: "container_1"
uid: "container1"
pod_uid: "ijkl"
namespace: "ns"
`

// Schema

var schemaInfoPB = `
name: "a_table"
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

// RegisterAgentRequest

var registerAgentRequestPB = `
register_agent_request {
  info {
    agent_id {
      data: "11285cdd1de94ab1ae6a0ba08c8c676c"
    }
    host_info {
      hostname: "test-host"
    }
  }
}
`

var invalidRegisterAgentRequestPB = `
register_agent_request {
  info {
    agent_id {
      data: "11285cdd1de94ab1ae6a0ba08c8c676c11285cdd1de94ab1ae6a0ba08c8c676c"
    }
    host_info {
      hostname: "test-host"
    }
  }
}
`

// UpdateAgentRequest

var updateAgentRequestPB = `
update_agent_request {
  info {
    agent_id {
      data: "11285cdd1de94ab1ae6a0ba08c8c676c"
    }
    host_info {
      hostname: "test-host"
    }
  }
}
`

var invalidUpdateAgentRequestPB = `
update_agent_request {
  info {
    agent_id {
      data: "11285cdd1de94ab1ae6a0ba08c8c676c11285cdd1de94ab1ae6a0ba08c8c676c"
    }
    host_info {
      hostname: "test-host"
    }
  }
}
`

// HeartbeatAck

var heartbeatAckPB = `
heartbeat_ack {
  time: 10
  update_info {
    updates {
      pod_update {
        uid:  "podUid"
        name: "podName"
      }
    }
    updates {
      pod_update {
        uid:  "podUid2"
        name: "podName2"
      }
    }
  }
}
`

// Heartbeat

var heartbeatPB = `
heartbeat {
  time: 1,
  agent_id: {
    data: "11285cdd1de94ab1ae6a0ba08c8c676c"
  }
  update_info {
    process_created {
      pid: 1
    }
  }
}
`

var invalidHeartbeatPB = `
heartbeat {
  time: 1,
  agent_id: {
    data: "11285cdd1de94ab1ae6a0ba08c8c676c"
  }
}
`

// Endpoint

const endpointsPb = `
subsets {
  addresses {
    ip: "127.0.0.1"
    hostname: "host"
    node_name: "this-is-a-node"
    target_ref {
      kind: "Pod"
      namespace: "pl"
      uid: "abcd"
    }
  }
  addresses {
    ip: "127.0.0.2"
    hostname: "host-2"
    node_name: "node-a"
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

const servicePb = `
metadata {
  name: "object_md"
  namespace: "a_namespace"
  uid: "ijkl"
  resource_version: "1",
  cluster_name: "a_cluster",
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

// Pod

const podPb = `
metadata {
  name: "object_md"
  uid: "ijkl"
  resource_version: "1",
  cluster_name: "a_cluster",
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
  conditions: 2
}
spec {
  node_name: "test"
  hostname: "hostname"
  dns_policy: 2
}
`

const podPbWithContainers = `
metadata {
  name: "object_md"
  uid: "ijkl"
  resource_version: "1",
  cluster_name: "a_cluster",
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
  conditions: 2
  container_statuses {
    name: "container1"
    container_state: 3
    container_id: "docker://test"
  }
  qos_class: QOS_CLASS_BURSTABLE
}
spec {
  node_name: "test"
  hostname: "hostname"
  dns_policy: 2
}
`
