<p align="center">

  [![Pixie!](./assets/readme_banner_v9.png)](https://pixielabs.ai)

</p>

<br>

[![Docs](https://img.shields.io/badge/docs-latest-blue)](https://withpixie.ai/docs)
[![Slack](https://slackin.withpixie.ai/badge.svg)](https://slackin.withpixie.ai)
[![Mentioned in Awesome Kubernetes](https://awesome.re/mentioned-badge.svg)](https://github.com/ramitsurana/awesome-kubernetes)
[![Mentioned in Awesome Go](https://awesome.re/mentioned-badge.svg)](https://github.com/avelino/awesome-go)
[![Build Status](https://travis-ci.org/pixie-labs/pixie.svg?branch=main)](https://travis-ci.org/pixie-labs/pixie)

<br clear="all">

## What is Pixie?

<img src="./assets/live_2oct20.gif" alt="Pixie" width="425" align="right">

Pixie gives you instant visibility by giving access to metrics, events, traces and logs without changing code.

We're building up Pixie for broad use by the end of 2020. If you are interested, feel free to try our [community beta](https://work.withpixie.ai/signup) and join our community on [slack](https://slackin.withpixie.ai/).

<br>

<details>
  <summary><strong>Table of contents</strong></summary>

- [Quick Start](#quick-start)
- [Demo](#get-instant-auto-telemetry)
- [Contributing](#contributing)
- [Open Source](#open-source)
- [Platform Architecture](#under-the-hood)
- [About Us](#about-us)
- [License](#license)

</details>


## Quick Start

#### Signup

Visit our [product page](https://work.withpixie.ai/) and signup with your google account.


#### Install CLI

Run the command below:

`bash -c "$(curl -fsSL https://withpixie.ai/install.sh)"`

Or see our [Installation Docs](https://docs.pixielabs.ai/installing-pixie/quick-start/#2.-install-the-cli) to install Pixie using Docker, Debian, RPM or with the latest binary. 

#### (optional) Setup a sandbox

If you don't already have a K8s cluster available, you can use Minikube to set-up a local environment:

- On Linux, run `minikube start --cpus=4 --memory=6000 --driver=kvm2 -p=<cluster-name>`. The default `docker` driver is not currently supported, so using the `kvm2` driver is important.

- On Mac, run `minikube start --cpus=4 --memory=6000 -p=<cluster-name>`.

More detailed instructions are available [here](https://docs.pixielabs.ai/installing-pixie/install-guides/minikube-setup/).

Start a demo-app:
- Deploy [Weaveworks'](https://www.weave.works/) [sock-shop](https://github.com/microservices-demo/microservices-demo) demo app by running `px demo deploy px-sock-shop`

#### 🚀 Deploy Pixie

Use the CLI to deploy the Pixie Platform in your K8s cluster by running:

px deploy

Alternatively, you can deploy with [YAML](https://docs.pixielabs.ai/installing-pixie/install-schemes/yaml/) or [Helm](https://docs.pixielabs.ai/installing-pixie/install-schemes/helm/).

<br>

Check out our [install guides](https://docs.pixielabs.ai/installing-pixie/install-guides/) and [walkthrough videos](https://www.youtube.com/watch?v=iMh2f8abTYU) for alternate install schemes.


## Get Instant Auto-Telemetry

#### Run scripts with `px` CLI

<img src="/assets/http_data.svg" alt="CLI Demo" width="425" align="right">

<br> Service SLA:

`px run px/service_stats`

<br> Node health:

`px run px/node_stats`

<br> MySQL metrics:

`px run px/mysql_stats`

<br> Explore more scripts by running:

`px scripts list`

<br> Check out our [pxl_scripts](pxl_scripts) repo for more examples.

<br clear="all">

#### View machine generated dashboards with Live views

<img src="./assets/live_2oct20.gif" alt="CLI Demo" width="425" align="right">

The Pixie Platform auto-generates "Live View" dashboard to visualize script results.

You can view them by clicking on the URLs prompted by `px` or by visiting:

`https://work.withpixie.ai/live`

<br clear="all">


#### Pipe Pixie dust into any tool

<img src="./assets/./cli_demo.svg" alt="CLI Demo" width="425" align="right">

You can transform and pipe your script results into any other system or workflow by consuming `px` results with tools like [jq](https://stedolan.github.io/jq/).

Example with http_data:

`px run px/http_data -o json| jq -r .`

More examples [here](pxl_scripts)

<br>_To see more script examples and learn how to write your own, check out our [docs](https://work.withpixie.ai/docs) for more guides_

<br clear="all">

## Contributing
We're excited to have you contribute to Pixie. Our community has adopted the [Contributor Covenant](CODE_OF_CONDUCT.md) as its code of conduct, and we expect all participants to adhere to it. Please report any violations to <community@pixielabs.ai>. All code contributions require the [Contributor License Agreement](CLA.md). The CLA can be signed when creating your first PR.

There are many ways to contribute to Pixie:

- **Bugs:** Something not working as expected? [Send a bug report](https://github.com/pixie-labs/pixie/issues/new?template=Bug_report.md).
- **Features:** Need new Pixie capabilities? [Send a feature request](https://github.com/pixie-labs/pixie/issues/new?template=Feature_request.md).
- **Views & Scripts Requests:** Need help building a live view or pxl scripts? [Send a live view request](https://github.com/pixie-labs/pixie/issues/new?template=Live_view_request.md).
- **PxL Scripts:** PxL scripts are used to extend Pixie functionality. They are an excellent way to contribute to golden debugging workflows. Look [here](pxl_scripts/README.md#Contributing) for more information.
- **Pixienaut Community:** Interested in becoming a [Pixienaut](https://github.com/pixie-labs/pixie/tree/master/pixienauts) and in helping shape our community? [Apply here](https://pixielabs.ai/community/).


## Open Source

Along with building Pixie as a freemium SaaS product, contributing open and accessible projects to the broader developer community is integral to our roadmap.

We plan to contribute in two ways:

- **Open Sourced Pixie Platform Primitives:** We plan to open-source components of the Pixie Platform which can be independently useful to developers after our Beta. These include our Community Pxl Scripts, Pixie CLI, eBPF Collectors etc. If you are interested in contributing during our Beta, [email us](mailto:community@pixielabs.ai).
- **Unlimited Pixie Community Access:** Our [Pixie Community](https://work.withpixie.ai/) product is a completely free offering with all core features of the Pixie developer experience. We will invest in this offering for the long term to give developers across the world an easy and zero cost way to use Pixie.

## Under the Hood

Three fundamental innovations enable Pixie's magical developer experience:

**Progressive Instrumentation:** Pixie Edge Modules (“PEMs”) collect full body request traces (via eBPF), system metrics & K8s events without the need for code-changes and at less than 5% overhead. Custom metrics, traces & logs can be integrated into the Pixie Command Module.

**In-Cluster Edge Compute:** The Pixie Command Module is deployed in your K8s cluster to isolate data storage and computation within your environment for drastically better intelligence, performance & security.

**Command Driven Interfaces:** Programmatically access data via the Pixie CLI and Pixie UI which are designed ground-up to allow you to run analysis & debug scenarios faster than any other developer tool.

_For more information on Pixie Platform's architecture, check out our [docs](https://work.withpixie.ai/docs) or [overview deck](https://docsend.com/view/kj38d76)_

## Resources

- [Company Website](https://pixielabs.ai)
- [Documentation](https://work.withpixie.ai/docs)
- [Community Slack](https://slackin.withpixie.ai/)
- [Issue Tracker](https://github.com/pixie-labs/pixie/issues)
- [Community Overview](https://github.com/pixie-labs/pixie/tree/master/pixienauts)
- [Youtube](https://www.youtube.com/channel/UCOMCDRvBVNIS0lCyOmst7eg/videos)
- [Overview Slide Deck](https://docsend.com/view/kj38d76)

## About Us

Founded in late 2018, we are a San Francisco based stealth machine intelligence startup. Our north star is to build a new generation of intelligent products which empower developers to engineer the future.

We're heads down building Pixie and excited to share it broadly with the community later this year. If you're interested in learning more about us or in our current job openings, we'd love to [hear from you](mailto:info@pixielabs.ai).


## License

Pixie Community is the free offering of Pixie's proprietary SaaS product catalogue.

Our [PxL Scripts](pxl_scripts/README.md) are licensed under [Apache License, Version 2.0](pxl_scripts/LICENSE.md).

Other Pixie Platform components such as Pixie CLI and eBPF based Data Collectors will also be licensed under the Apache License, Version 2.0. Contribution of these are planned for Oct 2020.
