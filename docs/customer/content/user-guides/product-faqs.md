---
title: "FAQs"
metaTitle: "User Guides| Pixie"
metaDescription: "Commonly asked questions about to deploy and use Pixie"
---


#### Can you give me a high level overview of the company?

You can review this [slide deck](https://docsend.com/view/r9s8wxe) for an overview of our story. 


#### Does Pixie support Thrift?

We currently do not support Thrift and are prioritizing support for HTTP and gRPC traffic.

#### Does Pixie support SSL/TLS traffic? 

We are currently working on our support for encrypted in-cluster traffic and plan to release it in Beta in Q1'20.

#### Does Pixie plan to consume Prometheus metrics?

Ingesting data from Prometheus is part of our roadmap but is not currently supported. 

#### Does Pixie have an API to access data?

We are working on a data access API and will be releasing it to Beta soon. In the interim, you can use the CLI's query support to use as an API replacement.