# ShareLaTeX
There are three different repos that contain different deployments for ShareLaTeX.
Markup : 1. Overleaf: https://github.com/overleaf/overleaf
         2. Sieve ShareLaTeX as distributed docker images: https://github.com/sieve-microservices/sharelatex-docker
         3. Docker ShareLaTeX-full: https://github.com/rigon/docker-sharelatex-full

Currently, we have a deployment with Option 3 from above. This contains the full ShareLaTeX application but all the
microservices are not deployed as individual services on k8s.

TODO(kgandhi):
Markup : 1. Use the distributed version of microservices. These aren't readily available as docker-compose.yamls that
           can be directly converted to kubernetes manifests.
         2. Compile the services and create docker images that can be deployed. (Look into options 1 and 2).

# Converting docker-compose.yml to k8s manifests

Kompose is used to convert docker-compose.yamls into kubernetes manifests.
```
curl -L https://github.com/kubernetes-incubator/kompose/releases/download/v0.7.0/kompose-linux-amd64 -o kompose
kompose convert -v
```

# Deploying on GKE

Create a cluster:
```
<pixielabs-repo>/scripts/create_gcp_dev_cluster.sh <cluster-name> -b -n <num_nodes> -i UBUNTU
```
Set context:
```
kubectl config use-context <context-name for new cluster>
```
Deploy k8s manifests
```
kubectl create -f ./kubernetes_manifests
```

# Next Steps
Markup: 1. Deploy ShareLaTeX as distributed services.
        2. Work on load generation. https://github.com/sieve-microservices/sharelatex-loadgenerator
