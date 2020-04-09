<p align="center">

  ![Pixie!](pixie_banner_light.png)

  <p align="center">
    Live-debug distributed environments with <a href="https://work.withpixie.ai/">Pixie</a>
  </p>

</p>

---
[![Slack](https://slackin.withpixie.ai/badge.svg)](https://slackin.withpixie.ai)

Pixie gives engineers access to no-instrumentation, streaming & unsampled auto-telemetry to debug performance issues in real-time.

We're building up Pixie for broad use by the end of 2020 with an initial focus on Kubernetes workloads. If you are interested, feel free to [try our beta](https://withpixie.ai/) and join our [community on slack](https://slackin.withpixie.ai/).

<details>
  <summary><strong>Table of contents</strong></summary>

- [Quick Start](#quick-start)
- [Run Scripts](#run-scripts)
- [Demos](#Demos)
- [Contributing](#contributing)
- [Open Source](#open-source)
- [Under the Hood](#under-the-hood)
- [About Us](#about-us)
</details>


## Quick start

1. **Create account**

    Visit our [product page](https://work.withpixie.ai/) and signup with your google account.

2. **Install**

    Copy and run this command to install the CLI in your environment.

    `bash -c "$(curl -fsSL https://withpixie.ai/install.sh)"`

3. **Deploy**

    Run this to deploy PEMs and the Pixie Command Module in your K8s cluster.

    `px deploy`

4. **Lift off! 🚀**

    Start using Pixie by running `px help` in your terminal or view your preconfigured dashboards in the [Live UI](https://work.withpixie.ai/)

_For detailed information on compatibility & requirements check out our [docs](https://work.withpixie.ai/docs/getting-started/compatibility-requirements) or ping us on [slack](https://slackin.withpixie.ai/)_


## Run Scripts

Once installed, you can access data in seconds.

1. **Deploy a Demo App**

    Run this to deploy the [sock-shop](https://github.com/microservices-demo/microservices-demo) demo app by [Weaveworks](https://www.weave.works/):

    `px demo deploy px-sock-shop`

    _Note: Skip this step, if you have pre-existing workloads running that you want to monitor._

2. **Run Scripts**

    Service SLA `px run px/service_stats`

    Node health: `px run px/node_stats`

    MySQL metrics: `px run px/node_stats`

    Explore more scripts by running `px scripts list` or by hitting cmd+k in the [Live UI](https://work.withpixie.ai/) 
    
    Check out our [pxl_scripts](pxl_scripts) repo for more examples.

3. **View and Itegrate Results**

    Live UI visualizes thee results for you: 

    `https://work.withpixie.ai/live`

    
    You can transform and pipe your script output into any other system or workflow:

    `px run px/pod_memory_usage -o json| jq -r .`


_To see more script examples and learn how to write your own, check out our [docs](https://work.withpixie.ai/docs) for more guides_

## Demos

**Install Pixie in seconds**

[![Pixie Deploy Overview](https://img.youtube.com/vi/iMh2f8abTYU/0.jpg)](https://www.youtube.com/watch?v=iMh2f8abTYU)


**Use `px` to list/run scripts and consuming results with tools like [jq](https://stedolan.github.io/jq/). More examples [here](pxl_scripts).**

![CLI Demo](./cli_demo.svg)


**Switch to live UI to visualize results and debug fast**

![CLI Demo](./live_7apr20.gif)


_For more demos, check out our [youtube](https://www.youtube.com/channel/UCOMCDRvBVNIS0lCyOmst7eg/videos)._

## Contributing

- **Bugs:** Something not working as expected? [Send a bug report](https://github.com/pixie-labs/pixie/issues/new?template=Bug_report.md).
- **Features:** Need new Pixie capabilities? [Send a feature request](https://github.com/pixie-labs/pixie/issues/new?template=Feature_request.md).
- **Views & Scripts:** Need help building a live views or pxl scripts? [Send a live view request](https://github.com/pixie-labs/pixie/issues/new?template=Live_view_request.md).
- **Community:** Interested in becoming a Pixienaut and in helping shape our community? [Email us](mailto:community@pixielabs.ai).


## Open Source

Along with building Pixie as a freemium SaaS product, contributing open and accessible projects to the broader developer community is integral to our roadmap. We plan to contribute in two ways:

- **Open Sourced Pixie Platform Primitives:** We plan to open-source components of the Pixie Platform which can be independently useful to developers after our Beta. These include our Community Pxl Scripts, Pixie CLI, eBPF Collectors etc. If you are interested in contributing during our Beta, [email us](mailto:community@pixielabs.ai).
- **Unlimited Pixie Community Access:** Our [Pixie Community](https://work.withpixie.ai/) product is a completely free offering with all core features of the Pixie developer experience. We will invest in this offering for the long term to give developers across the world an easy and zero cost way to use Pixie.

## Under the Hood

Three fundamental innovations enable Pixie's magical developer experience:

**Progressive Instrumentation:** Pixie Edge Modules (“PEMs”) collect full body request traces (via eBPF), system metrics & K8s events without the need for code-changes and at less than 5% overhead. Custom metrics, traces & logs can be integrated into the Pixie Command Module.

**In-Cluster Edge Compute:** The Pixie Command Module is deployed in your K8s cluster to isolate data storage and computation within your environment for drastically better intelligence, performance & security.

**Command Driven Interfaces:** Programmatically access data via the Pixie CLI and Pixie UI which are designed ground-up to allow you to run analysis & debug scenarios faster than any other developer tool.

_For more information on Pixie Platform's architecture, check out our [docs](https://work.withpixie.ai/docs) or [overview deck](https://docsend.com/view/kj38d76)_

## Resources

- [Documentation](https://work.withpixie.ai/docs)
- [Community Slack](https://slackin.withpixie.ai/)
- [Issue Tracker](https://github.com/pixie-labs/pixie/issues)
- [Youtube](https://www.youtube.com/channel/UCOMCDRvBVNIS0lCyOmst7eg/videos)
- [Overview Slide Deck](https://docsend.com/view/kj38d76)
- [Company Website](https://pixielabs.ai)

## About Us

Founded in late 2018, we are a San Francisco based stealth machine intelligence startup. Our north star is to build a new generation of intelligent products which empower developers to engineer the future.

We're heads down building Pixie and excited to share it broadly with the community later this year. If you're interested in learning more about us or in our current job openings, we'd love to [hear from you](mailto:info@pixielabs.ai).
