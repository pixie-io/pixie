# Docker Container
Location: gcr.io/pl-dev-infra/demos/superset:0.28.1
This docker container was built from apache incubator-superset
https://github.com/apache/incubator-superset

# Generating Docker Images
The superset application has a Dockerfile that is included in the github
repo and also has a shell script to build the docker image.
Run the following commands to generate the docker image for
the application and store in our container registry.
*[Original installation superset instructions](https://superset.incubator.apache.org/installation.html)*

```
# Run in this directory. Change path to docker-init.sh in the cp command if you run this in a different directory.
git clone https://github.com/apache/incubator-superset/
cd incubator-superset
cp contrib/docker/{docker-build.sh,docker-compose.yml,docker-entrypoint.sh,docker-init.sh,Dockerfile} .
cp contrib/docker/superset_config.py superset/
# copy over our docker-init.sh. If you don't do this, deployment will fail.
cp ../docker-init.sh .
bash -x docker-build.sh
docker images
docker tag apache/incubator-superset:latest gcr.io/pl-dev-infra/demos/superset:<version>
docker push gcr.io/pl-dev-infra/demos/superset:<version>
```

Note: You want to tag the image with the latest version of the application.

If you clone the latest git repo for the application, then you will need to update the
tag for the kubernetes manifest yaml files as well. This can be achieved as follows
(in the application's kubernetes_manifests directory):
```
sed -i'.original' -e 's/<old-version>/<new-version>/g' *yaml
```
You should commit these changes into the repo after testing out the changes
on GKE.

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
`//pixielabs.ai/pixielabs/demos/applications/superset/kubernetes_manifests/*.yaml`

Run the following command after setting the kubectl context for a cluster

```
kubectl create -f ./kubernetes_manifests
```

# Accessing the application

The superset service has been exposed to only the Pixie Labs office IP.
You may not be able to access the service anywhere else. To get the external ip:

```
kubectl get services superset
NAME       TYPE           CLUSTER-IP     EXTERNAL-IP     PORT(S)          AGE
superset   LoadBalancer   10.7.249.138   35.224.177.88   8088:31141/TCP   18m
```

Load the external ip address in a browser and specify the port as 8088.
http://35.224.177.88:8088
You should be able to log in as the admin user and browse the application.
