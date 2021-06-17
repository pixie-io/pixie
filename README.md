<p align="center">

  [![Pixie!](./.readme_assets/readme_banner_v9.png)](https://px.dev)

</p>

<br>

[![Docs](https://img.shields.io/badge/docs-latest-blue)](https://docs.px.dev)
[![Slack](https://slackin.px.dev/badge.svg)](https://slackin.px.dev)
[![Mentioned in Awesome Kubernetes](https://awesome.re/mentioned-badge.svg)](https://github.com/ramitsurana/awesome-kubernetes)
[![Mentioned in Awesome Go](https://awesome.re/mentioned-badge.svg)](https://github.com/avelino/awesome-go)
[![Build Status](https://jenkins.corp.pixielabs.ai/buildStatus/icon?job=pixie-oss%2Fbuild-and-test-all)](https://jenkins.corp.pixielabs.ai/job/pixie-oss/job/build-and-test-all/)
[![codecov](https://codecov.io/gh/pixie-labs/pixie/branch/main/graph/badge.svg?token=UG7P3QE5PQ)](https://codecov.io/gh/pixie-labs/pixie)

<br clear="all">

## What is Pixie?

<img src="./.readme_assets/live_2oct20.gif" alt="Pixie" width="425" align="right">

Pixie gives you instant visibility by giving access to metrics, events, traces and logs without changing code.

Try out [Pixie](https://docs.px.dev/installing-pixie/quick-start/) and join our community on [slack](https://slackin.px.dev/).

<br>

<details>
  <summary><strong>Table of contents</strong></summary>

- [Quick Start](#quick-start)
- [Demo](#get-instant-auto-telemetry)
- [Contributing](#contributing)
- [Platform Architecture](#under-the-hood)
- [Resources](#resources)
- [About Us](#about-us)
- [License](#license)

</details>

## Quick Start

Check out Pixie's [Quick Start](https://docs.px.dev/installing-pixie/quick-start) install guide.

## Get Instant Auto-Telemetry

#### Run scripts with `px` CLI

<img src="/.readme_assets/http_data.svg" alt="CLI Demo" width="425" align="right">

<br> Service SLA:

`px run px/service_stats`

<br> Node health:

`px run px/node_stats`

<br> MySQL metrics:

`px run px/mysql_stats`

<br> Explore more scripts by running:

`px scripts list`

<br> Check out our [pxl_scripts](src/pxl_scripts) folder for more examples.

<br clear="all">

#### View machine generated dashboards with Live views

<img src="./.readme_assets/live_2oct20.gif" alt="CLI Demo" width="425" align="right">

The Pixie Platform auto-generates "Live View" dashboards to visualize script results.

<br clear="all">

#### Pipe Pixie dust into any tool

<img src="./.readme_assets/cli_demo.svg" alt="CLI Demo" width="425" align="right">

You can transform and pipe your script results into any other system or workflow by consuming `px` results with tools like [jq](https://stedolan.github.io/jq/).

Example with http_data:

`px run px/http_data -o json| jq -r .`

More examples [here](src/pxl_scripts)

<br>_To see more script examples and learn how to write your own, check out our [docs](https://docs.px.dev) for more guides._

<br clear="all">

## Contributing

We are excited to have you contribute to Pixie! Before contributing, please read our [contribution guide](CONTRIBUTING.md).

## Under the Hood

Three fundamental innovations enable Pixie's magical developer experience:

**Progressive Instrumentation:** Pixie Edge Modules (“PEMs”) collect full body request traces (via eBPF), system metrics & K8s events without the need for code-changes and at less than 5% overhead. Custom metrics, traces & logs can be integrated into the Pixie Command Module.

**In-Cluster Edge Compute:** The Pixie Command Module is deployed in your K8s cluster to isolate data storage and computation within your environment for drastically better intelligence, performance & security.

**Command Driven Interfaces:** Programmatically access data via the Pixie CLI and Pixie UI which are designed ground-up to allow you to run analysis & debug scenarios faster than any other developer tool.

_For more information on Pixie Platform's architecture, check out our [docs](https://docs.px.dev/about-pixie/what-is-pixie/)._

## Resources

- [Website](https://px.dev)
- [Documentation](https://docs.px.dev)
- [Community Slack](https://slackin.px.dev/)
- [Issue Tracker](https://github.com/pixie-labs/pixie/issues)
- [Youtube](https://www.youtube.com/channel/UCOMCDRvBVNIS0lCyOmst7eg/videos)

## About Us

Pixie was built by a San Francisco based startup, Pixie Labs, Inc. [New Relic, Inc.](https://newrelic.com) acquired Pixie Labs in December 2020. New Relic, Inc. is [in the process of contributing](https://github.com/cncf/toc/issues/651) Pixie to the [Cloud Native Compute Foundation](https://www.cncf.io/).

## License

Pixie is licensed under [Apache License, Version 2.0](LICENSE).
