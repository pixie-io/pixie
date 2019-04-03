DIR="$( cd "$( dirname "${BASH_SOURCE[0]}"  )" && pwd  )"
kubectl create configmap grafana-import-dashboards \
  --namespace monitoring \
  --from-file=$DIR/k8s-pod-resources-dashboard.json \
  --from-file=$DIR/sock-shop-analytics-dashboard.json \
  --from-file=$DIR/prometheus-datasource.json               \
  --from-file=$DIR/sock-shop-performance-dashboard.json \
  --from-file=$DIR/prometheus-stats-dashboard.json          \
  --from-file=$DIR/sock-shop-resources-dashboard.json
