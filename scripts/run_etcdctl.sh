#!/bin/bash -ex

endpoint="https://localhost:2379"
cert="/etc/etcdtls/operator/etcd-tls/etcd-client.crt"
key="/etc/etcdtls/operator/etcd-tls/etcd-client.key"
cacert="/etc/etcdtls/operator/etcd-tls/etcd-client-ca.crt"

kubectl exec -t -i \
 $(kubectl get pod --namespace pl \
    --selector="app=etcd,etcd_cluster=pl-etcd" --output jsonpath='{.items[0].metadata.name}') \
 -n=pl -- /bin/sh -c \
 "ETCDCTL_API=3 etcdctl \
  --endpoints=$endpoint \
  --cert=$cert \
  --key=$key \
  --cacert=$cacert \
  $1"
