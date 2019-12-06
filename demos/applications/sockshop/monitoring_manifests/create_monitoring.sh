#!/bin/bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}"  )" && pwd  )"
gcloud_user_email=$(gcloud info --format='value(config.account)')
kubectl create clusterrolebinding system:aggregated-metrics-reader --clusterrole=cluster-admin \
 --user "$gcloud_user_email"
kubectl create -f "$DIR"
kubectl create -f "$DIR"/prometheus/prometheus-crb.yml
kubectl create -f "$DIR"/prometheus
bash "$DIR"/grafana/create_config_map.sh
kubectl create -f "$DIR"/grafana
