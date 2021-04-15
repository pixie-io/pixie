# Vizier microservices base specs

Lists specs of all of the microservices that comprise `vizier`, the monitoring software suite
deployed to k8s clusters.

# PEM container configuration

Stirling, which uses BPF, has a number of container environment requirements to run properly.
These requirements are reflected in `pem_daemonset.yaml`.
See src/stirling/README.md, under the section "Stirling docker container environment",
for an explanation of these requirements.
