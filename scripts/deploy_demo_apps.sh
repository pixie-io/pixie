#!/usr/bin/env bash

workspace=$(bazel info workspace 2> /dev/null)

kubectl apply -f ${workspace}/demos/applications/sockshop/kubernetes_manifests/sock-shop-ns.yaml && sleep 5
kubectl apply -f ${workspace}/demos/applications/sockshop/kubernetes_manifests
kubectl apply -f ${workspace}/demos/applications/sockshop/load_generation
