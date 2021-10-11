[![Pixie!](./.readme_assets/pixie-horizontal-color.png)](https://px.dev)

<br>

[![Docs](https://img.shields.io/badge/docs-latest-blue)](https://docs.px.dev)
[![Slack](https://slackin.px.dev/badge.svg)](https://slackin.px.dev)
[![Twitter](https://img.shields.io/twitter/url/https/twitter.com/pixie_run.svg?style=social&label=Follow%20%40pixie_run)](https://twitter.com/pixie_run)
[![Mentioned in Awesome Kubernetes](https://awesome.re/mentioned-badge.svg)](https://github.com/ramitsurana/awesome-kubernetes)
[![Mentioned in Awesome Go](https://awesome.re/mentioned-badge.svg)](https://github.com/avelino/awesome-go)
[![Build Status](https://jenkins.corp.pixielabs.ai/buildStatus/icon?job=pixie-oss%2Fbuild-and-test-all)](https://jenkins.corp.pixielabs.ai/job/pixie-oss/job/build-and-test-all/)
[![codecov](https://codecov.io/gh/pixie-io/pixie/branch/main/graph/badge.svg?token=UG7P3QE5PQ)](https://codecov.io/gh/pixie-io/pixie)
[![FOSSA Status](https://app.fossa.com/api/projects/custom%2B26327%2Fgithub.com%2Fpixie-io%2Fpixie.svg?type=shield)](https://app.fossa.com/projects/custom%2B26327%2Fgithub.com%2Fpixie-io%2Fpixie?ref=badge_shield)

<br>

Pixie is an open source observability tool for Kubernetes applications. Use Pixie to view the high-level state of your cluster (service maps, cluster resources, application traffic) and also drill-down into more detailed views (pod state, flame graphs, individual full-body application requests).

## Why Pixie?

Three features enable Pixie's magical developer experience:

- **Auto-telemetry:** Pixie uses eBPF to automatically collect telemetry data such as full-body requests, resource and network metrics, application profiles, and more. See the full list of data sources [here](https://docs.px.dev/about-pixie/data-sources/).

- **In-Cluster Edge Compute:** Pixie collects, stores and queries all telemetry data locally in the cluster. Pixie uses less than 5% of cluster CPU, and in most cases less than 2%.

- **Scriptability:** [PxL](https://docs.px.dev/reference/pxl/), Pixie’s flexible Pythonic query language, can be used across Pixie’s UI, CLI, and client APIs.

## Use Cases

### Network Monitoring

<img src=".readme_assets/net_flow_graph.png" alt="Network Flow Graph" width="525" align="right">

<br>

Use Pixie to monitor your network, including:

- The flow of network traffic within your cluster.
- The flow of DNS requests within your cluster.
- Individual full-body DNS requests and responses.
- A Map of TCP drops and TCP retransmits across your cluster.

<br>

For more details, check out the [tutorial](https://docs.px.dev/tutorials/pixie-101/network-monitoring/) or [watch](https://youtu.be/qIxzIPBhAUI) an overview.

<br clear="all">

### Infrastructure Health

<img src=".readme_assets/nodes.png" alt="Infrastructure Monitoring" width="525" align="right">

<br>

Monitor your infrastructure alongside your network and application layer, including:

- Resource usage by Pod, Node, Namespace.
- CPU flamegraphs per Pod, Node.

<br>

For more details, check out the [tutorial](https://docs.px.dev/tutorials/pixie-101/infra-health/) or [watch](https://youtu.be/2dFIpiBryu8) an overview.

<br clear="all">

### Service Performance

<img src=".readme_assets/service.png" alt="Service Performance" width="525" align="right">

<br>

Pixie automatically traces a [variety of protocols](https://docs.px.dev/about-pixie/data-sources/). Get immediate visibility into the health of your services, including:

- The flow of traffic between your services.
- Latency per service and endpoint.
- Sample of the slowest requests for an individual service.

<br>

For more details, check out the [tutorial](https://docs.px.dev/tutorials/pixie-101/service-performance/) or [watch](https://youtu.be/Rex0yz_5vwc) an overview.

<br clear="all">

### Database Query Profiling

<img src=".readme_assets/sql_query.png" alt="Database Query Profilling" width="525" align="right">

<br>

Pixie automatically traces a number of different [database protocols](https://docs.px.dev/about-pixie/data-sources/#supported-protocols). Use Pixie to monitor the performance of your database requests:

- Latency, error and throughput (LET) rate for all pods.
- LET rate per normalized query.
- Latency per individual full body query.
- Individual full-body requests and responses.

<br>

For more details, check out the [tutorial](https://docs.px.dev/tutorials/pixie-101/database-query-profiling/) or [watch](https://youtu.be/5NkU--hDXRQ) an overview.

<br clear="all">

### Request Tracing

<img src=".readme_assets/http_data_filtered.png" alt="Request Tracing" width="525" align="right">

<br>

Pixie makes debugging this communication between microservices easy by providing immediate and deep (full-body) visibility into requests flowing through your cluster. See:

- Full-body requests and response for [supported protocols](https://docs.px.dev/about-pixie/data-sources/#supported-protocols).
- Error rate per Service, Pod.

<br>

For more details, check out the [tutorial](https://docs.px.dev/tutorials/pixie-101/request-tracing/) or [watch](https://youtu.be/Gl0so4rbwno) an overview.

<br clear="all">

### Continuous Application Profiling

<img src=".readme_assets/pod_flamegraph.png" alt="Continuous Application Profiling" width="525" align="right">

<br>

Use Pixie's continuous profiling feature to identify performance issues within application code.

<br>

For more details, check out the [tutorial](https://docs.px.dev/tutorials/pixie-101/profiler/) or [watch](https://youtu.be/Zr-s3EvAey8) an overview.

<br clear="all">

### Distributed bpftrace Deployment

Use Pixie to deploy a [bpftrace](https://github.com/iovisor/bpftrace) program to all of the nodes in your cluster. After deploying the program, Pixie captures the output into a table and makes the data available to be queried and visualized int he Pixie UI. TCP Drops pictured. For more details, check out the [tutorial](https://docs.px.dev/tutorials/custom-data/distributed-bpftrace-deployment/) or [watch](https://youtu.be/xT7OYAgIV28) an overview.

### Dynamic Go Logging

Debug Go binaries deployed in production environments without needing to recompile and redeploy. For more details, check out the [tutorial](https://docs.px.dev/tutorials/custom-data/dynamic-go-logging/) or [watch](https://youtu.be/aH7PHSsiIPM) an overview.

<br clear="all">

## Get Started

<img src=".readme_assets/http_data.svg" alt="Request Tracing" width="525" align="right">

It takes just a few minutes to install Pixie. To get started, check out the [Install Guides](https://docs.px.dev/installing-pixie/install-guides/).

<br>

Once installed, you can interact with Pixie using the:

- [Web-based Live UI](https://docs.px.dev/using-pixie/using-live-ui/)
- [CLI](https://docs.px.dev/using-pixie/using-cli/)
- [API](https://docs.px.dev/using-pixie/api-quick-start/)

<br clear="all">

## Get Involved

Pixie is a community driven project; we welcome your contribution! For code contributions, please read our [contribution guide](CONTRIBUTING.md).

- File a [GitHub issue](https://github.com/pixie-io/pixie/issues) to report a bug or request a feature.
- Join our [Slack](https://slackin.px.dev) for live conversations and quick questions.
- Follow us on [Twitter](https://twitter.com/pixie_run) and [YouTube](https://www.youtube.com/channel/UCOMCDRvBVNIS0lCyOmst7eg).
- Join our monthly [community meetings](https://px.dev/community/#events).
- Provide feedback on our [roadmap](https://docs.px.dev/about-pixie/roadmap/).

<br clear="all">

## About Pixie

Pixie was contributed by [New Relic, Inc.](https://newrelic.com/) to the [Cloud Native Computing Foundation](https://www.cncf.io/) as a Sandbox project in June 2021.

## License

Pixie is licensed under [Apache License, Version 2.0](LICENSE).
