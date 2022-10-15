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

package goplanner_test

import (
	"os"
	"testing"

	"github.com/gofrs/uuid"
	"github.com/gogo/protobuf/proto"
	log "github.com/sirupsen/logrus"

	"px.dev/pixie/src/api/proto/uuidpb"
	"px.dev/pixie/src/api/proto/vizierpb"
	"px.dev/pixie/src/carnot/goplanner"
	"px.dev/pixie/src/carnot/planner/distributedpb"
	"px.dev/pixie/src/carnot/planner/plannerpb"
	"px.dev/pixie/src/carnot/udfspb"
	"px.dev/pixie/src/common/base/statuspb"
	"px.dev/pixie/src/pixie_cli/pkg/vizier"
	"px.dev/pixie/src/table_store/schemapb"
	"px.dev/pixie/src/utils"
	"px.dev/pixie/src/utils/script"
	funcs "px.dev/pixie/src/vizier/funcs/go"
	"px.dev/pixie/src/vizier/services/query_broker/controllers"
)

// Base to use. Must set asid and agent_id.
const pemCarnotInfoPb = `
has_grpc_server: false
has_data_store: true
processes_data: true
accepts_remote_sources: false
table_info {
	table: "table"
}
`

const basePlannerState = `
distributed_state {
	carnot_info {
		query_broker_address: "kelvin"
		agent_id {
			data: "00000001-0000-0000-0000-000000000000"
		}
		grpc_address: "1111"
		has_grpc_server: true
		has_data_store: false
		processes_data: true
		accepts_remote_sources: true
		asid: 0
	}
	schema_info {
		name: "table1"
		relation {
			columns {
				column_name: "time_"
				column_type: TIME64NS
				column_semantic_type: ST_NONE
			}
			columns {
				column_name: "cpu_cycles"
				column_type: INT64
				column_semantic_type: ST_NONE
			}
			columns {
				column_name: "upid"
				column_type: UINT128
				column_semantic_type: ST_NONE
			}
		}
		agent_list {
			data: "00000001-0000-0000-0000-000000000001"
		}
		agent_list {
			data: "00000001-0000-0000-0000-000000000002"
		}
		agent_list {
			data: "00000001-0000-0000-0000-000000000003"
		}
	}
}`

const pxClusterPxl = `
''' Cluster Overview

This view lists the namespaces and the nodes that are available on the current cluster.

'''
import px


ns_per_ms = 1000 * 1000
ns_per_s = 1000 * ns_per_ms
# Window size to use on time_ column for bucketing.
window_ns = px.DurationNanos(10 * ns_per_s)
# Flag to filter out requests that come from an unresolvable IP.
filter_unresolved_inbound = True
# Flag to filter out health checks from the data.
filter_health_checks = True
# Flag to filter out ready checks from the data.
filter_ready_checks = True


def nodes_for_cluster(start_time: str):
    ''' Gets a list of nodes in the current cluster since 'start_time'.
    Args:
    @start: Start time of the data to examine.
    '''
    df = px.DataFrame(table='process_stats', start_time=start_time)
    df.node = df.ctx['node_name']
    df.pod = df.ctx['pod_name']
    agg = df.groupby(['node', 'pod']).agg()
    nodes = agg.groupby('node').agg(pod_count=('pod', px.count))
    process_stats = process_stats_by_entity(start_time, 'node')
    output = process_stats.merge(nodes, how='inner', left_on='node', right_on='node',
                                 suffixes=['', '_x'])
    return output[['node', 'cpu_usage', 'pod_count']]


def process_stats_by_entity(start_time: str, entity: str):
    ''' Gets the windowed process stats (CPU, memory, etc) per node or pod.
    Args:
    @start: Starting time of the data to examine.
    @entity: Either pod or node_name.
    '''
    df = px.DataFrame(table='process_stats', start_time=start_time)
    df[entity] = df.ctx[entity]
    df.timestamp = px.bin(df.time_, window_ns)
    # First calculate CPU usage by process (UPID) in each k8s_object
    # over all windows.
    df = df.groupby([entity, 'upid', 'timestamp']).agg(
        rss=('rss_bytes', px.mean),
        vsize=('vsize_bytes', px.mean),
        # The fields below are counters, so we take the min and the max to subtract them.
        cpu_utime_ns_max=('cpu_utime_ns', px.max),
        cpu_utime_ns_min=('cpu_utime_ns', px.min),
        cpu_ktime_ns_max=('cpu_ktime_ns', px.max),
        cpu_ktime_ns_min=('cpu_ktime_ns', px.min),
        read_bytes_max=('read_bytes', px.max),
        read_bytes_min=('read_bytes', px.min),
        write_bytes_max=('write_bytes', px.max),
        write_bytes_min=('write_bytes', px.min),
        rchar_bytes_max=('rchar_bytes', px.max),
        rchar_bytes_min=('rchar_bytes', px.min),
        wchar_bytes_max=('wchar_bytes', px.max),
        wchar_bytes_min=('wchar_bytes', px.min),
    )
    # Next calculate cpu usage and memory stats per window.
    df.cpu_utime_ns = df.cpu_utime_ns_max - df.cpu_utime_ns_min
    df.cpu_ktime_ns = df.cpu_ktime_ns_max - df.cpu_ktime_ns_min
    df.read_bytes = df.read_bytes_max - df.read_bytes_min
    df.write_bytes = df.write_bytes_max - df.write_bytes_min
    df.rchar_bytes = df.rchar_bytes_max - df.rchar_bytes_min
    df.wchar_bytes = df.wchar_bytes_max - df.wchar_bytes_min
    # Sum by UPID.
    df = df.groupby([entity, 'timestamp']).agg(
        cpu_ktime_ns=('cpu_ktime_ns', px.sum),
        cpu_utime_ns=('cpu_utime_ns', px.sum),
        read_bytes=('read_bytes', px.sum),
        write_bytes=('write_bytes', px.sum),
        rchar_bytes=('rchar_bytes', px.sum),
        wchar_bytes=('wchar_bytes', px.sum),
        rss=('rss', px.sum),
        vsize=('vsize', px.sum),
    )
    df.actual_disk_read_throughput = df.read_bytes / window_ns
    df.actual_disk_write_throughput = df.write_bytes / window_ns
    df.total_disk_read_throughput = df.rchar_bytes / window_ns
    df.total_disk_write_throughput = df.wchar_bytes / window_ns
    # Now take the mean value over the various timestamps.
    df = df.groupby(entity).agg(
        cpu_ktime_ns=('cpu_ktime_ns', px.mean),
        cpu_utime_ns=('cpu_utime_ns', px.mean),
        actual_disk_read_throughput=('actual_disk_read_throughput', px.mean),
        actual_disk_write_throughput=('actual_disk_write_throughput', px.mean),
        total_disk_read_throughput=('total_disk_read_throughput', px.mean),
        total_disk_write_throughput=('total_disk_write_throughput', px.mean),
        avg_rss=('rss', px.mean),
        avg_vsize=('vsize', px.mean),
    )
    # Finally, calculate total (kernel + user time)  percentage used over window.
    df.cpu_usage = px.Percent((df.cpu_ktime_ns + df.cpu_utime_ns) / window_ns)
    return df.drop(['cpu_ktime_ns', 'cpu_utime_ns'])


def pods_for_cluster(start_time: str):
    ''' A list of pods in 'namespace'.
    Args:
    @start_time: The timestamp of data to start at.
    @namespace: The name of the namespace to filter on.
    '''
    df = px.DataFrame(table='process_stats', start_time=start_time)
    df.pod = df.ctx['pod_name']
    df.node = df.ctx['node_name']
    df.container = df.ctx['container_name']
    df = df.groupby(['pod', 'node', 'container']).agg()
    df = df.groupby(['pod', 'node']).agg(container_count=('container', px.count))
    df.start_time = px.pod_name_to_start_time(df.pod)
    df.status = px.pod_name_to_status(df.pod)
    process_stats = process_stats_by_entity(start_time, 'pod')
    output = process_stats.merge(df, how='inner', left_on='pod', right_on='pod',
                                 suffixes=['', '_x'])
    return output[['pod', 'cpu_usage', 'total_disk_read_throughput',
                   'total_disk_write_throughput', 'container_count',
                   'node', 'start_time', 'status']]


def namespaces_for_cluster(start_time: str):
    ''' Gets a overview of namespaces in the current cluster since 'start_time'.
    Args:
    @start: Start time of the data to examine.
    '''
    df = px.DataFrame(table='process_stats', start_time=start_time)
    df.service = df.ctx['service_name']
    df.pod = df.ctx['pod_name']
    df.namespace = df.ctx['namespace']
    agg = df.groupby(['service', 'pod', 'namespace']).agg()
    pod_count = agg.groupby(['namespace', 'pod']).agg()
    pod_count = pod_count.groupby('namespace').agg(pod_count=('pod', px.count))
    svc_count = agg.groupby(['namespace', 'service']).agg()
    svc_count = svc_count.groupby('namespace').agg(service_count=('service', px.count))
    pod_and_svc_count = pod_count.merge(svc_count, how='inner',
                                        left_on='namespace', right_on='namespace',
                                        suffixes=['', '_x'])
    process_stats = process_stats_by_entity(start_time, 'namespace')
    output = process_stats.merge(pod_and_svc_count, how='inner', left_on='namespace',
                                 right_on='namespace', suffixes=['', '_y'])
    return output[['namespace', 'pod_count', 'service_count', 'avg_vsize', 'avg_rss']]


def services_for_cluster(start_time: str):
    ''' Get an overview of the services in the current cluster.
    Args:
    @start_time: The timestamp of data to start at.
    '''
    df = px.DataFrame(table='process_stats', start_time=start_time)
    df.service = df.ctx['service']
    df = df[df.service != '']
    df.pod = df.ctx['pod']
    df = df.groupby(['service', 'pod']).agg()
    df = df.groupby('service').agg(pod_count=('pod', px.count))
    service_let = inbound_service_let_summary(start_time)
    joined = df.merge(service_let, how='inner', left_on='service', right_on='service',
                      suffixes=['', '_x'])
    return joined.drop('service_x')


def inbound_service_let_summary(start_time: str):
    ''' Compute a summary of traffic by requesting service, for requests
        on services in the current cluster..
    Args:
    @start_time: The timestamp of data to start at.
    '''
    df = inbound_service_let_helper(start_time)
    df = df[df.remote_addr != '']
    df.responder = df.service
    per_ns_df = df.groupby(['timestamp', 'service']).agg(
        throughput_total=('latency', px.count),
        inbound_bytes_total=('req_size', px.sum),
        outbound_bytes_total=('resp_size', px.sum)
    )
    per_ns_df.request_throughput = per_ns_df.throughput_total / window_ns
    per_ns_df.inbound_throughput = per_ns_df.inbound_bytes_total / window_ns
    per_ns_df.outbound_throughput = per_ns_df.inbound_bytes_total / window_ns
    per_ns_df = per_ns_df.groupby('service').agg(
        request_throughput=('request_throughput', px.mean),
        inbound_throughput=('inbound_throughput', px.mean),
        outbound_throughput=('outbound_throughput', px.mean)
    )
    quantiles_df = df.groupby('service').agg(
        latency=('latency', px.quantiles)
        error_rate=('failure', px.mean),
    )
    quantiles_df.error_rate = px.Percent(quantiles_df.error_rate)
    joined = per_ns_df.merge(quantiles_df, left_on='service',
                             right_on='service', how='inner',
                             suffixes=['', '_x'])
    return joined[['service', 'latency', 'request_throughput', 'error_rate',
                   'inbound_throughput', 'outbound_throughput']]


def inbound_service_let_helper(start_time: str):
    ''' Compute the let as a timeseries for requests received or by services in 'namespace'.
    Args:
    @start_time: The timestamp of data to start at.
    @namespace: The namespace to filter on.
    @groupby_cols: The columns to group on.
    '''
    df = px.DataFrame(table='http_events', start_time=start_time)
    df.service = df.ctx['service']
    df.pod = df.ctx['pod_name']
    df = df[df.service != '']
    df.latency = df.resp_latency_ns
    df.timestamp = px.bin(df.time_, window_ns)
    df.req_size = px.Bytes(px.length(df.req_body))
    df.resp_size = px.Bytes(px.length(df.resp_body))
    df.failure = df.resp_status >= 400
    filter_out_conds = ((df.req_path != '/health' or not filter_health_checks) and (
        df.req_path != '/readyz' or not filter_ready_checks)) and (
        df['remote_addr'] != '-' or not filter_unresolved_inbound)
    df = df[filter_out_conds]
    return df


def inbound_let_service_graph(start_time: str):
    ''' Compute a summary of traffic by requesting service, for requests on services
        in the current cluster. Similar to 'inbound_let_summary' but also breaks down
        by pod in addition to service.
    Args:
    @start_time: The timestamp of data to start at.
    '''
    df = inbound_service_let_helper(start_time)
    df = df.groupby(['timestamp', 'service', 'remote_addr', 'pod']).agg(
        latency_quantiles=('latency', px.quantiles),
        error_rate=('failure', px.mean),
        throughput_total=('latency', px.count),
        inbound_bytes_total=('req_size', px.sum),
        outbound_bytes_total=('resp_size', px.sum)
    )
    df.latency_p50 = px.DurationNanos(px.floor(px.pluck_float64(df.latency_quantiles, 'p50')))
    df.latency_p90 = px.DurationNanos(px.floor(px.pluck_float64(df.latency_quantiles, 'p90')))
    df.latency_p99 = px.DurationNanos(px.floor(px.pluck_float64(df.latency_quantiles, 'p99')))
    df = df[df.remote_addr != '']
    df.responder_pod = df.pod
    df.requestor_pod_id = px.ip_to_pod_id(df.remote_addr)
    df.requestor_pod = px.pod_id_to_pod_name(df.requestor_pod_id)
    df.responder_service = df.service
    df.requestor_service = px.pod_id_to_service_name(df.requestor_pod_id)
    df.request_throughput = df.throughput_total / window_ns
    df.inbound_throughput = df.inbound_bytes_total / window_ns
    df.outbound_throughput = df.outbound_bytes_total / window_ns
    df.error_rate = px.Percent(df.error_rate)
    return df.groupby(['responder_pod', 'requestor_pod', 'responder_service',
                       'requestor_service']).agg(
        latency_p50=('latency_p50', px.mean),
        latency_p90=('latency_p90', px.mean),
        latency_p99=('latency_p99', px.mean),
        request_throughput=('request_throughput', px.mean),
        error_rate=('error_rate', px.mean),
        inbound_throughput=('inbound_throughput', px.mean),
        outbound_throughput=('outbound_throughput', px.mean),
        throughput_total=('throughput_total', px.sum)
    )
`

const pxClusterJSON = `
{
  "variables": [
    {
      "name": "start_time",
      "type": "PX_STRING",
      "description": "The relative start time of the window. Current time is assumed to be now",
      "defaultValue": "-5m"
    }
  ],
  "widgets": [
    {
      "name": "Service Graph",
      "position": {
        "x": 0,
        "y": 0,
        "w": 12,
        "h": 3
      },
      "func": {
        "name": "inbound_let_service_graph",
        "args": [
          {
            "name": "start_time",
            "variable": "start_time"
          }
        ]
      },
      "displaySpec": {
        "@type": "types.px.dev/px.vispb.RequestGraph",
        "requestorPodColumn": "requestor_pod",
        "responderPodColumn": "responder_pod",
        "requestorServiceColumn": "requestor_service",
        "responderServiceColumn": "responder_service",
        "p50Column": "latency_p50",
        "p90Column": "latency_p90",
        "p99Column": "latency_p99",
        "errorRateColumn": "error_rate",
        "requestsPerSecondColumn": "request_throughput",
        "inboundBytesPerSecondColumn": "inbound_throughput",
        "outboundBytesPerSecondColumn": "outbound_throughput",
        "totalRequestCountColumn": "throughput_total"
      }
    },
    {
      "name": "Nodes",
      "position": {
        "x": 0,
        "y": 3,
        "w": 6,
        "h": 3
      },
      "func": {
        "name": "nodes_for_cluster",
        "args": [
          {
            "name": "start_time",
            "variable": "start_time"
          }
        ]
      },
      "displaySpec": {
        "@type": "types.px.dev/px.vispb.Table"
      }
    },
    {
      "name": "Namespaces",
      "position": {
        "x": 6,
        "y": 3,
        "w": 6,
        "h": 3
      },
      "func": {
        "name": "namespaces_for_cluster",
        "args": [
          {
            "name": "start_time",
            "variable": "start_time"
          }
        ]
      },
      "displaySpec": {
        "@type": "types.px.dev/px.vispb.Table"
      }
    },
    {
      "name": "Services",
      "position": {
        "x": 0,
        "y": 6,
        "w": 12,
        "h": 3
      },
      "func": {
        "name": "services_for_cluster",
        "args": [
          {
            "name": "start_time",
            "variable": "start_time"
          }
        ]
      },
      "displaySpec": {
        "@type": "types.px.dev/px.vispb.Table"
      }
    },
    {
      "name": "Pods",
      "position": {
        "x": 0,
        "y": 9,
        "w": 12,
        "h": 3
      },
      "func": {
        "name": "pods_for_cluster",
        "args": [
          {
            "name": "start_time",
            "variable": "start_time"
          }
        ]
      },
      "displaySpec": {
        "@type": "types.px.dev/px.vispb.Table"
      }
    }
  ]
}
`
const pxPodPxl = `
'''Pod Overview

Overview of a specific Pod monitored by Pixie with its high level application metrics
(latency, error-rate & rps) and resource usage (cpu, writes, reads).

'''

import px

ns_per_ms = 1000 * 1000
ns_per_s = 1000 * ns_per_ms
# Window size to use on time_ column for bucketing.
window_ns = px.DurationNanos(10 * ns_per_s)
# Flag to filter out requests that come from an unresolvable IP.
filter_unresolved_inbound = True
# Flag to filter out health checks from the data.
filter_health_checks = True
# Flag to filter out ready checks from the data.
filter_ready_checks = True


def containers(start_time: str, pod: px.Pod):
    ''' A list of containers in 'pod'.

    Args:
    @start_time: The timestamp of data to start at.
    @pod: The name of the pod to filter on.

    '''
    df = px.DataFrame(table='process_stats', start_time=start_time)
    df = df[df.ctx['pod'] == pod]
    df.name = df.ctx['container_name']
    df.id = df.ctx['container_id']
    df = df.groupby(['name', 'id']).agg()
    df.status = px.container_id_to_status(df.id)
    return df


def node(start_time: str, pod: px.Pod):
    ''' A list containing the node the 'pod' is running on.

    Args:
    @start_time: The timestamp of data to start at.
    @pod: The name of the pod to filter on.

    '''
    df = px.DataFrame(table='process_stats', start_time=start_time)
    df = df[df.ctx['pod'] == pod]
    df.node = df.ctx['node_name']
    df.service = df.ctx['service']
    df.pod_id = df.ctx['pod_id']
    df.pod_name = df.ctx['pod']
    df = df.groupby(['node', 'service', 'pod_id', 'pod_name']).agg()
    df.pod_start_time = px.pod_name_to_start_time(df.pod_name)
    df.status = px.pod_name_to_status(df.pod_name)
    return df.drop('pod_name')


def processes(start_time: str, pod: px.Pod):
    ''' A list of processes in 'pod'.

    Args:
    @start_time: The timestamp of data to start at.
    @pod: The name of the pod to filter on.

    '''
    df = px.DataFrame(table='process_stats', start_time=start_time)
    df = df[df.ctx['pod'] == pod]
    df.cmd = df.ctx['cmdline']
    df.pid = df.ctx['pid']
    df = df.groupby(['pid', 'cmd', 'upid']).agg()
    return df


def resource_timeseries(start_time: str, pod: px.Pod):
    ''' Compute the resource usage as a timeseries for 'pod'.

    Args:
    @start_time: The timestamp of data to start at.
    @pod: The name of the pod to filter on.

    '''
    df = px.DataFrame(table='process_stats', start_time=start_time)
    df = df[df.ctx['pod'] == pod]
    df.timestamp = px.bin(df.time_, window_ns)
    df.container = df.ctx['container_name']

    # First calculate CPU usage by process (UPID) in each k8s_object
    # over all windows.
    df = df.groupby(['upid', 'container', 'timestamp']).agg(
        rss=('rss_bytes', px.mean),
        vsize=('vsize_bytes', px.mean),
        # The fields below are counters, so we take the min and the max to subtract them.
        cpu_utime_ns_max=('cpu_utime_ns', px.max),
        cpu_utime_ns_min=('cpu_utime_ns', px.min),
        cpu_ktime_ns_max=('cpu_ktime_ns', px.max),
        cpu_ktime_ns_min=('cpu_ktime_ns', px.min),
        read_bytes_max=('read_bytes', px.max),
        read_bytes_min=('read_bytes', px.min),
        write_bytes_max=('write_bytes', px.max),
        write_bytes_min=('write_bytes', px.min),
        rchar_bytes_max=('rchar_bytes', px.max),
        rchar_bytes_min=('rchar_bytes', px.min),
        wchar_bytes_max=('wchar_bytes', px.max),
        wchar_bytes_min=('wchar_bytes', px.min),
    )

    # Next calculate cpu usage and memory stats per window.
    df.cpu_utime_ns = df.cpu_utime_ns_max - df.cpu_utime_ns_min
    df.cpu_ktime_ns = df.cpu_ktime_ns_max - df.cpu_ktime_ns_min
    df.actual_disk_read_throughput = (df.read_bytes_max - df.read_bytes_min) / window_ns
    df.actual_disk_write_throughput = (df.write_bytes_max - df.write_bytes_min) / window_ns
    df.total_disk_read_throughput = (df.rchar_bytes_max - df.rchar_bytes_min) / window_ns
    df.total_disk_write_throughput = (df.wchar_bytes_max - df.wchar_bytes_min) / window_ns

    # Then aggregate process individual process metrics.
    df = df.groupby(['timestamp', 'container']).agg(
        cpu_ktime_ns=('cpu_ktime_ns', px.sum),
        cpu_utime_ns=('cpu_utime_ns', px.sum),
        actual_disk_read_throughput=('actual_disk_read_throughput', px.sum),
        actual_disk_write_throughput=('actual_disk_write_throughput', px.sum),
        total_disk_read_throughput=('total_disk_read_throughput', px.sum),
        total_disk_write_throughput=('total_disk_write_throughput', px.sum),
        rss=('rss', px.sum),
        vsize=('vsize', px.sum),
    )

    # Finally, calculate total (kernel + user time)  percentage used over window.
    df.cpu_usage = px.Percent((df.cpu_ktime_ns + df.cpu_utime_ns) / window_ns)
    df['time_'] = df['timestamp']
    return df.drop(['cpu_ktime_ns', 'cpu_utime_ns', 'timestamp'])


def network_timeseries(start_time: str, pod: px.Pod):
    ''' Gets the network stats (transmitted/received traffic) for the input node.

    Args:
    @start: Starting time of the data to examine.
    @node: The full name of the node to filter on.
    '''
    df = px.DataFrame(table='network_stats', start_time=start_time)
    df = df[df.ctx['pod'] == pod]
    df.timestamp = px.bin(df.time_, window_ns)

    # First calculate network usage by node over all windows.
    # Data is sharded by Pod in network_stats.
    df = df.groupby(['timestamp', 'pod_id']).agg(
        rx_bytes_end=('rx_bytes', px.max),
        rx_bytes_start=('rx_bytes', px.min),
        tx_bytes_end=('tx_bytes', px.max),
        tx_bytes_start=('tx_bytes', px.min),
        tx_errors_end=('tx_errors', px.max),
        tx_errors_start=('tx_errors', px.min),
        rx_errors_end=('rx_errors', px.max),
        rx_errors_start=('rx_errors', px.min),
        tx_drops_end=('tx_drops', px.max),
        tx_drops_start=('tx_drops', px.min),
        rx_drops_end=('rx_drops', px.max),
        rx_drops_start=('rx_drops', px.min),
    )

    # Calculate the network statistics rate over the window.
    # We subtract the counter value at the beginning ('_start')
    # from the value at the end ('_end').
    df.rx_bytes_per_ns = (df.rx_bytes_end - df.rx_bytes_start) / window_ns
    df.tx_bytes_per_ns = (df.tx_bytes_end - df.tx_bytes_start) / window_ns
    df.rx_drops_per_ns = (df.rx_drops_end - df.rx_drops_start) / window_ns
    df.tx_drops_per_ns = (df.tx_drops_end - df.tx_drops_start) / window_ns
    df.rx_errors_per_ns = (df.rx_errors_end - df.rx_errors_start) / window_ns
    df.tx_errors_per_ns = (df.tx_errors_end - df.tx_errors_start) / window_ns

    # Add up the network values per node.
    df = df.groupby(['timestamp']).agg(
        rx_bytes_per_ns=('rx_bytes_per_ns', px.sum),
        tx_bytes_per_ns=('tx_bytes_per_ns', px.sum),
        rx_drop_per_ns=('rx_drops_per_ns', px.sum),
        tx_drops_per_ns=('tx_drops_per_ns', px.sum),
        rx_errors_per_ns=('rx_errors_per_ns', px.sum),
        tx_errors_per_ns=('tx_errors_per_ns', px.sum),
    )
    df['time_'] = df['timestamp']
    return df


def inbound_latency_timeseries(start_time: str, pod: px.Pod):
    ''' Compute the latency as a timeseries for requests received by 'pod'.

    Args:
    @start_time: The timestamp of data to start at.
    @pod: The name of the pod to filter on.

    '''
    df = let_helper(start_time)
    df = df[df.pod == pod]

    df = df.groupby(['timestamp']).agg(
        latency_quantiles=('latency', px.quantiles)
    )

    # Format the result of LET aggregates into proper scalar formats and
    # time series.
    df.latency_p50 = px.DurationNanos(px.floor(px.pluck_float64(df.latency_quantiles, 'p50')))
    df.latency_p90 = px.DurationNanos(px.floor(px.pluck_float64(df.latency_quantiles, 'p90')))
    df.latency_p99 = px.DurationNanos(px.floor(px.pluck_float64(df.latency_quantiles, 'p99')))
    df.time_ = df.timestamp
    return df[['time_', 'latency_p50', 'latency_p90', 'latency_p99']]


def inbound_request_timeseries_by_container(start_time: str, pod: px.Pod):
    ''' Compute the request statistics as a timeseries for requests received
        by 'pod' by container.

    Args:
    @start_time: The timestamp of data to start at.
    @pod: The name of the pod to filter on.

    '''
    df = let_helper(start_time)
    df = df[df.pod == pod]
    df.container = df.ctx['container']

    df = df.groupby(['timestamp', 'container']).agg(
        error_rate_per_window=('failure', px.mean),
        throughput_total=('latency', px.count)
    )

    # Format the result of LET aggregates into proper scalar formats and
    # time series.
    df.request_throughput = df.throughput_total / window_ns
    df.errors_per_ns = df.error_rate_per_window * df.request_throughput / px.DurationNanos(1)
    df.error_rate = px.Percent(df.error_rate_per_window)
    df.time_ = df.timestamp

    return df[['time_', 'container', 'request_throughput', 'errors_per_ns', 'error_rate']]


def inbound_let_summary(start_time: str, pod: px.Pod):
    ''' Gets a summary of requests inbound to 'pod'.

    Args:
    @start: Starting time of the data to examine.
    @pod: The pod to filter on.
    '''
    df = let_helper(start_time)
    df = df[df.pod == pod]

    quantiles_agg = df.groupby(['pod', 'remote_addr']).agg(
        latency=('latency', px.quantiles),
        total_request_count=('latency', px.count)
    )

    quantiles_table = quantiles_agg[['pod', 'remote_addr', 'latency',
                                     'total_request_count']]

    range_agg = df.groupby(['pod', 'remote_addr', 'timestamp']).agg(
        requests_per_window=('time_', px.count),
        error_rate=('failure', px.mean)
    )

    rps_table = range_agg.groupby(['pod', 'remote_addr']).agg(
        requests_per_window=('requests_per_window', px.mean),
        error_rate=('error_rate', px.mean)
    )

    joined_table = quantiles_table.merge(rps_table,
                                         how='inner',
                                         left_on=['pod', 'remote_addr'],
                                         right_on=['pod', 'remote_addr'],
                                         suffixes=['', '_x'])

    joined_table.error_rate = px.Percent(joined_table.error_rate)
    joined_table.request_throughput = joined_table.requests_per_window / window_ns

    joined_table.responder = df.pod
    joined_table.requestor = px.pod_id_to_pod_name(px.ip_to_pod_id(df.remote_addr))

    return joined_table[['requestor', 'remote_addr', 'latency',
                         'error_rate', 'request_throughput']]


def let_helper(start_time: str):
    ''' Compute the initial part of the let for requests.
        Filtering to inbound/outbound traffic by pod is done by the calling function.

    Args:
    @start_time: The timestamp of data to start at.

    '''
    df = px.DataFrame(table='http_events', start_time=start_time)
    df.pod = df.ctx['pod']
    df.latency = df.resp_latency_ns

    df.timestamp = px.bin(df.time_, window_ns)

    df.resp_size = px.length(df.resp_body)
    df.failure = df.resp_status >= 400
    filter_out_conds = ((df.req_path != '/health' or not filter_health_checks) and (
        df.req_path != '/readyz' or not filter_ready_checks)) and (
        df['remote_addr'] != '-' or not filter_unresolved_inbound)

    df = df[filter_out_conds]
    return df
`

const pxPodJSON = `
{
  "variables": [
    {
      "name": "start_time",
      "type": "PX_STRING",
      "description": "The relative start time of the window. Current time is assumed to be now",
      "defaultValue": "-5m"
    },
    {
      "name": "pod",
      "type": "PX_POD",
      "description": "The pod name to filter on. Format: <ns>/<pod_name>",
      "defaultValue": ""
    }
  ],
  "globalFuncs": [
    {
      "outputName": "inbound_latency",
      "func": {
        "name": "inbound_latency_timeseries",
        "args": [
          {
            "name": "start_time",
            "variable": "start_time"
          },
          {
            "name": "pod",
            "variable": "pod"
          }
        ]
      }
    },
    {
      "outputName": "inbound_requests",
      "func": {
        "name": "inbound_request_timeseries_by_container",
        "args": [
          {
            "name": "start_time",
            "variable": "start_time"
          },
          {
            "name": "pod",
            "variable": "pod"
          }
        ]
      }
    },
    {
      "outputName": "resource_timeseries",
      "func": {
        "name": "resource_timeseries",
        "args": [
          {
            "name": "start_time",
            "variable": "start_time"
          },
          {
            "name": "pod",
            "variable": "pod"
          }
        ]
      }
    },
    {
      "outputName": "network_timeseries",
      "func": {
        "name": "network_timeseries",
        "args": [
          {
            "name": "start_time",
            "variable": "start_time"
          },
          {
            "name": "pod",
            "variable": "pod"
          }
        ]
      }
    }
  ],
  "widgets": [
    {
      "name": "HTTP Requests",
      "globalFuncOutputName": "inbound_requests",
      "displaySpec": {
        "@type": "types.px.dev/px.vispb.TimeseriesChart",
        "timeseries": [
          {
            "value": "request_throughput",
            "mode": "MODE_AREA",
            "series": "container",
            "stackBySeries": true
          }
        ],
        "title": "",
        "yAxis": {
          "label": "request throughput"
        },
        "xAxis": null
      },
      "position": {
        "x": 0,
        "y": 0,
        "w": 4,
        "h": 3
      }
    },
    {
      "name": "HTTP Errors",
      "globalFuncOutputName": "inbound_requests",
      "displaySpec": {
        "@type": "types.px.dev/px.vispb.TimeseriesChart",
        "timeseries": [
          {
            "value": "errors_per_ns",
            "mode": "MODE_AREA",
            "series": "container",
            "stackBySeries": true
          }
        ],
        "title": "",
        "yAxis": {
          "label": "error rate"
        },
        "xAxis": null
      },
      "position": {
        "x": 4,
        "y": 0,
        "w": 4,
        "h": 3
      }
    },
    {
      "name": "HTTP Latency",
      "globalFuncOutputName": "inbound_latency",
      "displaySpec": {
        "@type": "types.px.dev/px.vispb.TimeseriesChart",
        "timeseries": [
          {
            "value": "latency_p50",
            "mode": "MODE_LINE"
          },
          {
            "value": "latency_p90",
            "mode": "MODE_LINE"
          },
          {
            "value": "latency_p99",
            "mode": "MODE_LINE"
          }
        ],
        "title": "",
        "yAxis": {
          "label": "milliseconds"
        },
        "xAxis": null
      },
      "position": {
        "x": 8,
        "y": 0,
        "w": 4,
        "h": 3
      }
    },
    {
      "name": "CPU Usage",
      "globalFuncOutputName": "resource_timeseries",
      "displaySpec": {
        "@type": "types.px.dev/px.vispb.TimeseriesChart",
        "timeseries": [
          {
            "value": "cpu_usage",
            "mode": "MODE_AREA",
            "series": "container",
            "stackBySeries": true
          }
        ],
        "title": "",
        "yAxis": {
          "label": "CPU usage"
        },
        "xAxis": null
      },
      "position": {
        "x": 0,
        "y": 3,
        "w": 4,
        "h": 3
      }
    },
    {
      "name": "Containers List",
      "func": {
        "name": "containers",
        "args": [
          {
            "name": "start_time",
            "variable": "start_time"
          },
          {
            "name": "pod",
            "variable": "pod"
          }
        ]
      },
      "displaySpec": {
        "@type": "types.px.dev/px.vispb.Table"
      },
      "position": {
        "x": 4,
        "y": 3,
        "w": 4,
        "h": 3
      }
    },
    {
      "name": "Process List",
      "func": {
        "name": "processes",
        "args": [
          {
            "name": "start_time",
            "variable": "start_time"
          },
          {
            "name": "pod",
            "variable": "pod"
          }
        ]
      },
      "displaySpec": {
        "@type": "types.px.dev/px.vispb.Table"
      },
      "position": {
        "x": 8,
        "y": 3,
        "w": 4,
        "h": 3
      }
    },
    {
      "name": "Network Sent and Received",
      "globalFuncOutputName": "network_timeseries",
      "displaySpec": {
        "@type": "types.px.dev/px.vispb.TimeseriesChart",
        "timeseries": [
          {
            "value": "rx_bytes_per_ns",
            "mode": "MODE_LINE"
          },
          {
            "value": "tx_bytes_per_ns",
            "mode": "MODE_LINE"
          }
        ],
        "title": "",
        "yAxis": {
          "label": "Network throughput"
        },
        "xAxis": null
      },
      "position": {
        "x": 0,
        "y": 6,
        "w": 4,
        "h": 3
      }
    },
    {
      "name": "Bytes Read",
      "globalFuncOutputName": "resource_timeseries",
      "displaySpec": {
        "@type": "types.px.dev/px.vispb.TimeseriesChart",
        "timeseries": [
          {
            "value": "total_disk_read_throughput",
            "mode": "MODE_AREA",
            "series": "container",
            "stackBySeries": true
          }
        ],
        "title": "",
        "yAxis": {
          "label": "Disk Read Throughput"
        },
        "xAxis": null
      },
      "position": {
        "x": 4,
        "y": 6,
        "w": 4,
        "h": 3
      }
    },
    {
      "name": "Bytes Written",
      "globalFuncOutputName": "resource_timeseries",
      "displaySpec": {
        "@type": "types.px.dev/px.vispb.TimeseriesChart",
        "timeseries": [
          {
            "value": "total_disk_write_throughput",
            "mode": "MODE_AREA",
            "series": "container",
            "stackBySeries": true
          }
        ],
        "title": "",
        "yAxis": {
          "label": "Disk Write Throughput"
        },
        "xAxis": null
      },
      "position": {
        "x": 8,
        "y": 6,
        "w": 4,
        "h": 3
      }
    },
    {
      "name": "Resident Set Size",
      "globalFuncOutputName": "resource_timeseries",
      "displaySpec": {
        "@type": "types.px.dev/px.vispb.TimeseriesChart",
        "timeseries": [
          {
            "value": "rss",
            "mode": "MODE_AREA",
            "series": "container",
            "stackBySeries": true
          }
        ],
        "title": "",
        "yAxis": {
          "label": "RSS"
        },
        "xAxis": null
      },
      "position": {
        "x": 0,
        "y": 9,
        "w": 4,
        "h": 3
      }
    },
    {
      "name": "Virtual Memory Size",
      "globalFuncOutputName": "resource_timeseries",
      "displaySpec": {
        "@type": "types.px.dev/px.vispb.TimeseriesChart",
        "timeseries": [
          {
            "value": "vsize",
            "mode": "MODE_AREA",
            "series": "container",
            "stackBySeries": true
          }
        ],
        "title": "",
        "yAxis": {
          "label": "vsize"
        },
        "xAxis": null
      },
      "position": {
        "x": 4,
        "y": 9,
        "w": 4,
        "h": 3
      }
    },
    {
      "name": "Inbound Traffic to Pod",
      "func": {
        "name": "inbound_let_summary",
        "args": [
          {
            "name": "start_time",
            "variable": "start_time"
          },
          {
            "name": "pod",
            "variable": "pod"
          }
        ]
      },
      "displaySpec": {
        "@type": "types.px.dev/px.vispb.Table"
      },
      "position": {
        "x": 0,
        "y": 12,
        "w": 12,
        "h": 3
      }
    },
    {
      "name": "Pod Metadata",
      "func": {
        "name": "node",
        "args": [
          {
            "name": "start_time",
            "variable": "start_time"
          },
          {
            "name": "pod",
            "variable": "pod"
          }
        ]
      },
      "displaySpec": {
        "@type": "types.px.dev/px.vispb.Table"
      },
      "position": {
        "x": 0,
        "y": 15,
        "w": 12,
        "h": 1
      }
    }
  ]
}
`

func makePlannerState(numPEM int) *distributedpb.LogicalPlannerState {
	plannerStatePB := new(distributedpb.LogicalPlannerState)
	err := proto.UnmarshalText(basePlannerState, plannerStatePB)
	if err != nil {
		return nil
	}
	baseCarnot := distributedpb.CarnotInfo{}
	err = proto.UnmarshalText(pemCarnotInfoPb, &baseCarnot)
	if err != nil {
		return nil
	}

	schemas := schemapb.Schema{}
	err = proto.UnmarshalText(schemaPB, &schemas)
	if err != nil {
		return nil
	}

	agentList := make([]*uuidpb.UUID, 0)
	for i := 1; i < numPEM+1; i++ {
		pem := baseCarnot
		u := uuid.Must(uuid.NewV4())
		pem.AgentID = utils.ProtoFromUUID(u)
		pem.ASID = uint32(i)

		plannerStatePB.DistributedState.CarnotInfo = append(plannerStatePB.DistributedState.CarnotInfo, &pem)
		agentList = append(agentList, pem.AgentID)
	}

	for i := range plannerStatePB.DistributedState.SchemaInfo {
		plannerStatePB.DistributedState.SchemaInfo[i].AgentList = agentList
	}

	for n, s := range schemas.RelationMap {
		newSchemaInfo := distributedpb.SchemaInfo{
			Name:      n,
			Relation:  s,
			AgentList: agentList,
		}
		plannerStatePB.DistributedState.SchemaInfo = append(plannerStatePB.DistributedState.SchemaInfo, &newSchemaInfo)
	}

	return plannerStatePB
}

func setupPlanner() (*goplanner.GoPlanner, error) {
	// Create the compiler.
	var udfInfoPb udfspb.UDFInfo
	u, err := funcs.Asset("src/vizier/funcs/data/udf.pb")
	if err != nil {
		return nil, err
	}
	err = proto.Unmarshal(u, &udfInfoPb)
	if err != nil {
		return nil, err
	}
	c, err := goplanner.New(&udfInfoPb)
	if err != nil {
		return nil, err
	}
	return &c, nil
}

func benchmarkPlannerInnerLoop(c *goplanner.GoPlanner, queryRequestPB *plannerpb.QueryRequest) {
	plannerResultPB, err := c.Plan(queryRequestPB)

	if err != nil {
		log.Fatalln("Failed to plan:", err)
		os.Exit(1)
	}

	status := plannerResultPB.Status
	if status.ErrCode != statuspb.OK {
		log.Fatalln("Failed to plan:", status.Msg)
		os.Exit(1)
	}
}
func benchmarkPlanner(b *testing.B, queryRequestPB *plannerpb.QueryRequest, numAgents int) {
	// Create the compiler.
	c, err := setupPlanner()
	if err != nil {
		log.Error(err)
		b.FailNow()
	}
	defer c.Free()

	// Make the planner state.
	queryRequestPB.LogicalPlannerState = makePlannerState(numAgents)

	// Run the benchmark loop.
	for i := 0; i < b.N; i++ {
		benchmarkPlannerInnerLoop(c, queryRequestPB)
	}
}

// BenchmarkSimple10Agents benchmarks compiling a simple query for 10 PEMs.
func BenchmarkSimple10Agents(b *testing.B) {
	queryRequestPB := &plannerpb.QueryRequest{
		QueryStr: "import px\ndf = px.DataFrame(table='table1')\npx.display(df, 'out')",
	}
	benchmarkPlanner(b, queryRequestPB, 10)
}

// BenchmarkSimple1000Agents benchmarks compiling a simple query for 1000 PEMs.
func BenchmarkSimple1000Agents(b *testing.B) {
	queryRequestPB := &plannerpb.QueryRequest{
		QueryStr: "import px\ndf = px.DataFrame(table='table1')\npx.display(df, 'out')",
	}
	benchmarkPlanner(b, queryRequestPB, 1000)
}

func getExecRequest(pxlScript, visJSON string) (*plannerpb.QueryRequest, error) {
	vs, err := script.ParseVisSpec(visJSON)
	if err != nil {
		return nil, err
	}
	funcs, err := vizier.GetFuncsToExecute(&script.ExecutableScript{Vis: vs})
	if err != nil {
		return nil, err
	}
	vpb := &vizierpb.ExecuteScriptRequest{
		QueryStr:  pxlScript,
		ExecFuncs: funcs,
	}
	return controllers.VizierQueryRequestToPlannerQueryRequest(vpb)
}

// BenchmarkPxCluster1000Agents benchmarks compiling px/cluster for 1000 PEMs.
func BenchmarkPxCluster1000Agents(b *testing.B) {
	queryRequestPB, err := getExecRequest(pxClusterPxl, pxClusterJSON)
	if err != nil {
		log.Error(err)
		b.FailNow()
	}
	benchmarkPlanner(b, queryRequestPB, 1000)
}

// BenchmarkPxPod1000Agents benchmarks compiling px/pod for 1000 PEMs.
func BenchmarkPxPod1000Agents(b *testing.B) {
	queryRequestPB, err := getExecRequest(pxPodPxl, pxPodJSON)
	if err != nil {
		log.Error(err)
		b.FailNow()
	}
	benchmarkPlanner(b, queryRequestPB, 1000)
}
