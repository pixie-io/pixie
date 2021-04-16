#!/bin/bash -ex

endpoint="https://localhost:2379"
cert="/clientcerts/etcd-client.crt"
key="/clientcerts/etcd-client.key"
cacert="/clientcerts/etcd-client-ca.crt"

kubectl exec -t -i \
 $(kubectl get pod --namespace px \
    --selector="vizier-dep=etcd" --output jsonpath='{.items[0].metadata.name}') \
 -n=pl -- /bin/sh -c \
 "ETCDCTL_API=3 etcdctl \
  --endpoints=$endpoint \
  --cert=$cert \
  --key=$key \
  --cacert=$cacert \
  $1"
