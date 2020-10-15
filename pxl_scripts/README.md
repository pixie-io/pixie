# PXL Scripts Overview

Pixie open sources all of its scripts, which serve as examples of scripting in the PxL language. To learn more about PxL, take a look at our [documentation](https://docs.pixielabs.ai/pxl).

- px/[agent_status.pxl](https://github.com/pixie-labs/pixie/tree/main/pxl_scripts/px/agent_status.pxl): Gets the status of all the Pixie agents (PEMs/Collectors) running.
- px/[cluster.pxl](https://github.com/pixie-labs/pixie/tree/main/pxl_scripts/px/cluster.pxl): Lists the namespaces and the node that are available on the current cluster.
- px/[cql_data.pxl](https://github.com/pixie-labs/pixie/tree/main/pxl_scripts/px/cql_data.pxl): Shows a sample of CQL (Cassandra) requests in the cluster.
- px/[cql_stats.pxl](https://github.com/pixie-labs/pixie/tree/main/pxl_scripts/px/cql_stats.pxl): Calculates the latency, error rate, and throughput of a pod's CQL (Cassandra) requests.
- px/[funcs.pxl](https://github.com/pixie-labs/pixie/tree/main/pxl_scripts/px/funcs.pxl): Lists all of the funcs available in Pixie.
- px/[http_data.pxl](https://github.com/pixie-labs/pixie/tree/main/pxl_scripts/px/http_data.pxl): Shows a sample of HTTP requests in the Cluster.
- px/[http_data_filtered.pxl](https://github.com/pixie-labs/pixie/tree/main/pxl_scripts/px/http_data_filtered.pxl): Shows a sample of HTTP requests in the Cluster filtered by service, pod & req-path.
- px/[http_post_requests.pxl](https://github.com/pixie-labs/pixie/tree/main/pxl_scripts/px/http_post_requests.pxl): Show a sample of HTTP requests in the Cluster that have method POST.
- px/[http_request_stats.pxl](https://github.com/pixie-labs/pixie/tree/main/pxl_scripts/px/http_request_stats.pxl): HTTP request statistics aggregated by Service.
- px/[jvm_data.pxl](https://github.com/pixie-labs/pixie/tree/main/pxl_scripts/px/jvm_data.pxl): JVM stats for Java processes running on the cluster.
- px/[jvm_stats.pxl](https://github.com/pixie-labs/pixie/tree/main/pxl_scripts/px/jvm_stats.pxl): Returns the JVM Stats per Pod. You can filter this by node.
- px/[largest_http_request.pxl](https://github.com/pixie-labs/pixie/tree/main/pxl_scripts/px/largest_http_request.pxl): Calculates the largest HTTP Request according to the passed in filter value.
- px/[most_http_data.pxl](https://github.com/pixie-labs/pixie/tree/main/pxl_scripts/px/most_http_data.pxl): Finds the endpoint on a specific Pod that passes the most HTTP Data. Optionally, you can uncomment a line to see a table summarizing data per service, endpoint pair.
- px/[mysql_data.pxl](https://github.com/pixie-labs/pixie/tree/main/pxl_scripts/px/mysql_data.pxl): Shows a sample of MySQL request in the cluster.
- px/[mysql_stats.pxl](https://github.com/pixie-labs/pixie/tree/main/pxl_scripts/px/mysql_stats.pxl): This live view calculates the latency, error rate, and throughput of a pod's MySQL requests.
- px/[namespace.pxl](https://github.com/pixie-labs/pixie/tree/main/pxl_scripts/px/namespace.pxl): This view gives a top-level summary of the pods and services in a given namespace, as well as a service map.
- px/[net_flow_graph.pxl](https://github.com/pixie-labs/pixie/tree/main/pxl_scripts/px/net_flow_graph.pxl): The set of k8s pods that talk to the specified IPs. Shows a graph and a table of the connections. Calculates total bandwidth for the lifetime of the connection.
- px/[network_stats.pxl](https://github.com/pixie-labs/pixie/tree/main/pxl_scripts/px/network_stats.pxl): Get network stats time series.
- px/[node.pxl](https://github.com/pixie-labs/pixie/tree/main/pxl_scripts/px/node.pxl): This view summarizes the process and network stats for a given input node in a cluster. It computes CPU, memory consumption, as well as network traffic statistics. It also displays a list of pods that were on that node during the time window.
- px/[nodes.pxl](https://github.com/pixie-labs/pixie/tree/main/pxl_scripts/px/nodes.pxl): This view summarizes the process and network stats for each node in a cluster. It computes CPU, memory consumption, as well as network traffic statistics, per node. It also displays a list of pods that were on each node during the time window.
- px/[pid_memory_usage.pxl](https://github.com/pixie-labs/pixie/tree/main/pxl_scripts/px/pid_memory_usage.pxl): Get the Virtual memory usage and average memory for all processes in the k8s cluster.
- px/[pixie_quality_metrics.pxl](https://github.com/pixie-labs/pixie/tree/main/pxl_scripts/px/pixie_quality_metrics.pxl): Metrics that sample Pixie's collector data.
- px/[pod.pxl](https://github.com/pixie-labs/pixie/tree/main/pxl_scripts/px/pod.pxl): Overview of a specific Pod monitored by Pixie with its high level application metrics (latency, error-rate & rps) and resource usage (cpu, writes, reads).
- px/[pod_edge_stats.pxl](https://github.com/pixie-labs/pixie/tree/main/pxl_scripts/px/pod_edge_stats.pxl): Gets pod latency, error rate and throughput according to another service. Edit the requestor filter to the name of the incoming service you want to filter by. Visualize these in three separate time series charts.
- px/[pod_lifetime_resource.pxl](https://github.com/pixie-labs/pixie/tree/main/pxl_scripts/px/pod_lifetime_resource.pxl): Total resource usage of a pod over it's lifetime.
- px/[pod_memory_usage.pxl](https://github.com/pixie-labs/pixie/tree/main/pxl_scripts/px/pod_memory_usage.pxl): Get the Virtual memory usage and average memory for all processes in the k8s cluster.
- px/[pods.pxl](https://github.com/pixie-labs/pixie/tree/main/pxl_scripts/px/pods.pxl): List of Pods monitored by Pixie in a given Namespace with their high level application metrics (latency, error-rate & rps) and resource usage (cpu, writes, reads).
- px/[psql_data.pxl](https://github.com/pixie-labs/pixie/tree/main/pxl_scripts/px/psql_data.pxl): Shows a sample of PostgreSQL request in the cluster.
- px/[psql_stats.pxl](https://github.com/pixie-labs/pixie/tree/main/pxl_scripts/px/psql_stats.pxl): This live view calculates the latency, error rate, and throughput of a pod's PostgreSQL requests.
- px/[schemas.pxl](https://github.com/pixie-labs/pixie/tree/main/pxl_scripts/px/schemas.pxl): Get all the table schemas available in the system.
- px/[service.pxl](https://github.com/pixie-labs/pixie/tree/main/pxl_scripts/px/service.pxl): This script gets an overview of an individual service, summarizing its request statistics.
- px/[service_edge_stats.pxl](https://github.com/pixie-labs/pixie/tree/main/pxl_scripts/px/service_edge_stats.pxl): Gets service latency, error rate and throughput according to another service. Edit the requestor filter to the name of the incoming service you want to filter by. Visualize these in three separate time series charts.
- px/[service_memory_usage.pxl](https://github.com/pixie-labs/pixie/tree/main/pxl_scripts/px/service_memory_usage.pxl): Get the Virtual memory usage and average memory for all services in the k8s cluster.
- px/[service_stats.pxl](https://github.com/pixie-labs/pixie/tree/main/pxl_scripts/px/service_stats.pxl): Gets service latency, error rate and throughput. Visualize them in three separate time series charts.
- px/[services.pxl](https://github.com/pixie-labs/pixie/tree/main/pxl_scripts/px/services.pxl): This script gets an overview of the services in a namespace, summarizing their request statistics.
- px/[slow_http_requests.pxl](https://github.com/pixie-labs/pixie/tree/main/pxl_scripts/px/slow_http_requests.pxl): This view shows a sample of slow requests by service.
- px/[tcp_retransmits.pxl](https://github.com/pixie-labs/pixie/tree/main/pxl_scripts/px/tcp_retransmits.pxl): Shows TCP retransmission counts in the cluster.
- px/[tracepoint_status.pxl](https://github.com/pixie-labs/pixie/tree/main/pxl_scripts/px/tracepoint_status.pxl): Returns information about tracepoints running on the cluster.
- px/[upids.pxl](https://github.com/pixie-labs/pixie/tree/main/pxl_scripts/px/upids.pxl): Returns information about tracepoints running on the cluster.


## Contributing

If you want to contribute a new PxL script, please discuss your idea on a Github [issue](https://github.com/pixie-labs/pixie/issues). Since the scripts are exposed to all community users there is a comprehensive review process.

You can contribute a PxL script by forking our repo, adding a new script then creating a pull request. Once the script is accepted it will automatically deploy once the CI process completes.

To learn in more details, you can review this tutorial on [Contributing PxL Scripts](https://docs.pixielabs.ai/tutorials/contributing-pxl-scripts)
