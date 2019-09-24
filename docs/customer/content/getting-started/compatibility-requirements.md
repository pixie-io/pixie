---
title: "Compatibility & Requirements"
metaTitle: "Getting Started | Pixie"
metaDescription: "Pixie is ..."
---

Pixie is designed for Linux environments on Kubernetes clusters.

## Compatibility

#### OS
|         | Support         | Version           |
|:------- | :-------------  | :-------------    |
| Linux   | Supported       | v4.14+            |
| Windows | Not Supported   | Not in roadmap    |

#### Container Orchestrator
|               | Support       | Version                   |
| :------------ | :------------ | :----------------------   |
| Kubernetes    | Supported     | V1.8+ (v1.12+ preferred)  |
| Docker Swarm  | Not Supported | Not in roadmap            |
| Nomad         | Not Supported | Not in roadmap            |
| Mesos-Marathon| Not Supported | Not in roadmap            | 

#### Network Protocols
|           | Support       | Notes                     |
| :-------- | :------------ | :----------------------   |
| HTTP      | Supported     |                           |
| HTTPS     | Not Supported | Planned in Beta roadmap   |
| gRPC      | Not Supported | Planned in Beta roadmap   |
| Thrift    | Not Supported | Not in roadmap            |

#### Databases

|               | Support       | Notes                     |
| :------------ | :------------ | :----------------------   |
| MySQL         | Not Supported | Planned in Beta roadmap   |
| PostgreSQL    | Not Supported | Planned in Beta roadmap   |
| MongoDB       | Not Supported | Planned in Beta roadmap   |
| Kafka         | Not Supported | Planned in Beta roadmap   |


## Requirements

#### Memory Requirements
|                       | Minimum   | Notes            |
| :-------------------  | :-------- | :--------------- |
| Pixie Edge Module     | 500Mi     | 2GiB  preferred  |
| Pixie Command Module  | 2GiB      | 8GiB+ preferred  |

#### CPU Requirements
We recommend configuring CPU limits for both the Pixie Edge Module and Pixie Command Module to be with 5% of the node's allocated CPU capacity.