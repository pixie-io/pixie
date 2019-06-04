# ShareLaTeX
There are three different repos that contain different deployments for ShareLaTeX.
Markup : 1. Overleaf: https://github.com/overleaf/overleaf
         2. Sieve ShareLaTeX as distributed docker images: https://github.com/sieve-microservices/sharelatex-docker
         3. Docker ShareLaTeX-full: https://github.com/rigon/docker-sharelatex-full

Options 1 and 3 above are the same and are deplyed as a monolith, i.e., all the services are
deployed as a single pod and all the services communicate internally. All the kubernetes manifest
files for this are under the monolith directory.

Option 2 is a distributed collection of services that communicate across nodes in a cluster.
All the kubernetes manifest files for this are under the distributed directory. The docker
containers for the distributed services can be created using the repo here:
https://github.com/pixie-labs/sharelatex


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

Note: One of the services (tags) requires the mongo service to be called mongodb, whereas others
expect it to be called mongo. We have added an additional service file which still refers to the
same instance of the mondo deployment. TODO(kgandhi): Modify the tag service to refer to mongo
service as mongo instead of mongodb.


# Next Steps
Markup: 1. Deploy ShareLaTeX as distributed services. (DONE)
        2. Fix bugs from 1 (Images and track changes not working correctly).
        3. Work on load generation. https://github.com/sieve-microservices/sharelatex-loadgenerator
