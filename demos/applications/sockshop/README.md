# Sock-shop demo

You can now deploy sock-shop with `px demo deploy px-sock-shop`. This command also deploy load
generator.

## Default monitoring
If you want to setup the `prometheus-grafana` monitoring setup that came with sock-shop, deploy
sock-shop directly with monitoring rather than using `px demo`.

```
kubectl create -f ./kubernetes_manifests
bash monitoring_manifests/create_monitoring.sh
```

You can find the ip to access the app at `EXTERNAL-IP`.

```
$ kubectl get services -n monitoring grafana
NAME      TYPE           CLUSTER-IP     EXTERNAL-IP   PORT(S)        AGE
grafana   LoadBalancer   10.47.252.60   10.32.0.8     80:31296/TCP   5d
```

NOTE: the `grafana` deployment includes a job to setup the dashboards. This can take a few minutes
to complete.  You can check the status of this by watching the pods setup.
