# Grafana Plugin Demo

This demo is intended to display a sample use of Pixie Datasource Plugin for Grafana. This folder contains manifests to deploy and configure a sample Grafana application which displays information about a sample cluster.

## Deploy Grafana Demo Application

1. Define your secret file. Example:
```
apiVersion: v1
kind: Secret
metadata:
    name: grafana-demo-secret
type: Opaque
stringData:
    PIXIE_API_KEY: api_key
```
2. Deploy demo application:
```
  kubectl apply -f PATH_TO_SECRET/grafana_plugin_secret.yaml
  kubectl apply -f PATH_TO_PIXIE_REPO/k8s/grafana_demo
```
Note:
Since the the ingress manifests are configured to work with Pixie GCP DNS, you will want to configure `grafana-ingress.yaml` with your DNS configurations.
If you don't want to use ingress configurations, you may delete `grafana-ingress.yaml`, and change `grafana-service.yaml` to change service type to `LoadBalancer`.
## Delete Grafana Demo Application

```
  kubectl delete -f PATH_TO_PIXIE_REPO/k8s/grafana_demo --ignore-not-found=true
```
