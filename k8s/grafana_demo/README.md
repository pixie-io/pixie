# Grafana Plugin Demo

This demo is intended to display a sample use of Pixie Datasource Plugin for Grafana. This folder contains manifests to deploy and configure a sample grafana application which displays information about a sample cluster.

## Deploy Grafana Demo Application

```
  kubectl apply -f PATH_TO_SECRET/grafana_plugin_secret.yaml
  kubectl apply -f pixielabs/demos/grafana-demo/
```

## Delete Grafana Demo Application

```
  kubectl delete -f pixielabs/demos/grafana-demo/ --ignore-not-found=true
```
