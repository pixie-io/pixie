---
title: "System Overview"
metaTitle: "Getting Started | Pixie"
metaDescription: "Pixie is ..."
---

#### What pain-point does Pixie address? 
Pixie is designed to reduce Meant Time to Isolation (MTTI) for performance incidents in production & staging environments. 

#### Who is Pixie designed for and how do they use it? 
Pixie is designed for two primary personas:
- **Application Developers** responsible for monitoring the health of services they own. 
- **On-Call Engineers** SRE, DevOps, SWEs etc., responsible for incident response.

Pixie's Beta releases will include:
- **Pre-Configured Dashboards** for passive health monitoring.
- **Live Debugger** for rapid analysis of live data.

![](/customer/src/components/images/pixiedocs_product_experience_v4.png?raw=true)

#### How does Pixie work? 
The underlying product architecture is designed to provide developers unsampled access to live telemetry (network spans, metrics & ci/cd events) with minimal instrumentation.

The three primary components of the system are: 
- **Pixie Edge Modules (PEM):** Deployed as DaemonSets, PEM's leverage Pixie's eBPF collector to collect network transactions and system metrics without any code changes.
- **Pixie Command Modules:** Deployed as a set of K8s services within the monitoried cluster, it is responsible for data aggregation, data integration and query execution. 
- **Pixie Console:** Developer facing web application which surfaces user-interfaces for debugging, adminstration & API management.

![](/customer/src/components/images/pixiedocs_system_architecture_v4.png?raw=true)