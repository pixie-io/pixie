#!/bin/bash
kubectl -n=spinnaker port-forward $(kubectl -n=spinnaker get po -l=cluster=spin-deck -o=jsonpath='{.items[0].metadata.name}') 9000 &
kubectl -n=spinnaker port-forward $(kubectl -n=spinnaker get po -l=cluster=spin-gate -o=jsonpath='{.items[0].metadata.name}') 8084