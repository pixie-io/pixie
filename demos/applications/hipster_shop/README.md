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

# How to set up a cluster on GKE to run the application
```
gcloud auth login
gcloud config set project pl-dev-infra
gcloud services enable container.googleapis.com
gcloud container clusters create <name for the cluster> --enable-autoupgrade \
    --enable-autoscaling --min-nodes=3 --max-nodes=10 --num-nodes=5 --zone=us-west1-a
gcloud container clusters get-credentials <name of the cluster> --zone=us-west1-a
```
The gcloud commands above will create a new cluster on which you can run the application.
You will also need to set the context for kubectl. You can do that with the following commands:
```
kubectl config view
kubectl config use-context gke_pl-dev-infra_us-west1-a_<name of the cluster>
```

# Load generation on GKE
We plan to use locust to generate variable loads on the application. We also want to isolate load
generation from affecting the system under test. Therefore, we need to restrict the deployment of
the load generator to a specific node. This node should also not schedule any other services to run on
it. This can be done by tainting the node to not schedule any services other than those that can  tolerate
the taint. Also, the load generation deployment or pod spec needs to target this specific node so that it is
not scheduled on any other available node. This can be achieved in three steps.

1. Taint one of the nodes in the cluster in the following way:
```
kubectl get nodes
kubectl taint nodes <node-name> dedicated=load:NoSchedule
```
The above command prevents any service from being deployed on this node unless it tolerates this taint.

2. Label the node that has been tainted with a key-value pair. The example (label) has already been specified in the
loadgenerator's kubernetes manifest:
```
kubectl label nodes <node-name> dedicatednode=load
```

3. In the load generator kubernetes manifest, we need to specify the taint
tolerance as well as the target node it is to be deployed on. Using the label in step 2,
we can specify the target node. In the kubernetes_manifests/loadgenerator.yaml, under the template spec:
```
nodeSelector:
    dedicatednode: load
tolerations:
    - key: "dedicated"
    operator: "Equal"
    value: "load"
    effect: "NoSchedule"
```

# How to Run the Application on GKE
The k8s manifest files are included in the folder for the application:
`//pixielabs.ai/pixielabs/demos/applications/hipster_shop/kubernetes_manifests/*.yaml`

Run the following command after setting the kubectl context for a cluster

```
cd //pixielabs.ai/pixielabs/demos/applications/hipster_shop/
kubectl create -f ./kubernetes_manifests
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
