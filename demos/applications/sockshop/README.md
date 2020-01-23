# Sock-shop demo
[Original](https://github.com/microservices-demo/microservices-demo)
The sock-shop demo deploy comes from the Weaveworks demo above. Here is the original
[K8S deploy folder](https://github.com/microservices-demo/microservices-demo/tree/master/deploy/kubernetes).

To run this deploy in GKE and not expose the app to the world, we had to add the following
lines to the LoadBalancer Service configs:
```
   annotations:
        cloud.google.com/load-balancer-type: "internal"
```
The two files where this was changed:
```
./kubernetes_manifests/front-end-svc.yaml
./monitoring_manifests/grafana/grafana-svc.yaml
```

## Preset user credential

You can login with the username `user` and password `password`. Note that they are chosen
intentionally to be easier to memorize.

## Running the demo on GKE
At the time of writing (Apr 2019), we had a GKE cluster running under the name `dev-cluster`.
(See the hipster-shop README to setup a cluster from scratch.)
For the rest of the guide, you'll need to be in the k8s context for the gke cluster.
```
# set the gke cluster as the k8s context.
kubectl config use-context gke_pl-dev-infra_us-west1-a_dev-cluster
```
Now to run the sock-shop demo on its own
```
kubectl create -f ./kubernetes_manifests
```
You should be able to find the ip to access sock shop by running the following:
```
$ kubectl get services -n sock-shop  sock-shop-front-end
NAME                  TYPE           CLUSTER-IP      EXTERNAL-IP   PORT(S)        AGE
sock-shop-front-end   LoadBalancer   10.47.250.246   10.32.0.106   80:30001/TCP   5d
```
The external-ip can be entered into the browser and you'll have access to the sock-shop app.

## Default monitoring
If you want to setup the prometheus-grafana monitoring setup that came with sock-shop,
run the following
```
bash monitoring_manifests/create_monitoring.sh
```
And you can find the ip to access the app at `EXTERNAL-IP`.
```
$ kubectl get services -n monitoring grafana
NAME      TYPE           CLUSTER-IP     EXTERNAL-IP   PORT(S)        AGE
grafana   LoadBalancer   10.47.252.60   10.32.0.8     80:31296/TCP   5d
```
NOTE: the grafana deployment includes a job to setup the dashboards. This can take a few minutes to complete.
You can check the status of this by watching the pods setup.

## Load Generation
We are using the load generation setup provdied with the original demo.
First, make sure that the host matches the ip that was found in the sock-shop setup above (NOT the monitoring ip).
Then edit `./load_generation/loadtest-dep.yaml` and replace the  host argument with the ip of the sock-shop frontend.

To start load_generation,
```
$ kubectl create -f ./load_generation
```
You should be able to see the loads increasing if you watch the gcp compute
usage or if you have setup the monitoring services.

In the future, we might want to adapt this setup using the
[suggestion here](https://stackoverflow.com/questions/37221483/kubernetes-service-located-in-another-namespace).
