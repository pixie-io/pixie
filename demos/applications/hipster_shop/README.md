# Docker Container
Location: gcr.io/pl-dev-infra/demos/microservices-demo-app/*
All the docker images for the individual components are in the above location
https://github.com/GoogleCloudPlatform/microservices-demo

# Generating Docker Images
The microservices demo application comes with support for skaffold.
To generate docker images that we can use, we can use the skaffold
build utility to generate the docker images. This can be done as follows:
```
git clone https://github.com/GoogleCloudPlatform/microservices-demo
cd microservices-demo
```

Edit the skaffold.yaml file to change the location of the docker images:
Change
```
gcr.io/microservices-demo-app/<service>
```
to
```
gcr.io/pl-dev-infra/demos/microservices-demo-app/<service>
```

Next, run:
```
skaffold build
```
to build the docker images and store them on our container registry.

Note: Based on when you clone the repo and generate the images, you will need to
update the tags in the kubernetes manaifest yaml files. This can be done in the following way:

From the cloned repo directory:
```
git rev-parse --short HEAD
```
This will provide a short tag for git hash (<new-hash>) when you cloned the repo.

Next, update the all the manifest yaml files with the command below
(in the application's kubernetes_manifests directory):
```
sed -i'.original' -e 's/<old-hash>/<new-hash>/g' *yaml
```
You should commit these changes into the repo after testing out the changes
on GKE.


# How to Run the Application on GKE
The k8s manifest files are included in the folder for the application:
`//pixielabs.ai/pixielabs/demos/applications/hipster_shop/kubernetes_manifests/*.yaml`

Run the following command after setting the kubectl context for a cluster

```
cd //pixielabs.ai/pixielabs/demos/applications/hipster_shop/
kubectl create -f ./
```

# Accessing the application

The hipster shop frontend service has been exposed to only the Pixie Labs office IP.
You may not be able to access the service anywhere else. To get the external ip:

```
kubectl get services frontend-external
NAME                TYPE           CLUSTER-IP    EXTERNAL-IP      PORT(S)        AGE
frontend-external   LoadBalancer   10.59.251.7   35.185.232.220   80:31921/TCP   16m
```

Load the external ip address in a browser
http://35.185.232.220

You should be able to use the application now.

# Load generation
The load generation kubernetes manifest has been purposely left at the
`hipster_shop` directory level to avoid starting this service since we want
to apply the load from another node pool. This way the load generator service
is not started by default.
