# Locust
We use Locust, a Python library, to simulate network traffic patterns on our demo applications.
See the "Demo Applications" design doc for further details about Locust and the wrapper we've written around Locust.

# Creating a Load
If you want to create a new load for a demo application, follow the instructions below.
## Locust File Template
Locust requires a locustFile which specifies the types and behaviors of users accessing the
application in the load test. Since the ratios and min/max wait times for the user types may change in each phase,
our wrapper needs a different locustFile for each phase.
To do so, you must provide a template version of the locustFile, which includes the tasks and behaviors of users,
but not the actual user class definitions.
For an example of a locustFile template, see:
```
/demos/applications/hipster_shop/load_generation/locustfile.loadgen.tmpl
```
In general, for consistency, your locustfile template should be saved in
`demos/applications/<application_name>/load_generation/locustfile.loadgen.tmpl`.

## Load Configuration
Our Locust wrapper expects a configuration file to be provided in the form of a pbtxt file.
This file details the phase configurations for the load, and specifies the path to the locust file template.
See `demos/load_generation/proto/load_config.proto` for the protobuf definitions.

For each user type, you must have the corresponding user behavior defined in your locustFile template.
For example, for the user:
```
  user_types {
    name: "Weekend"
    ratio: 2
    min_wait_ms: 1000
    max_wait_ms: 1000
  }
```
The locustFile template should have a `class WeekendUserBehavior(TaskSet):` defined.

For an example config, see:
```
/demos/applications/hipster_shop/load_generation/load.pbtxt
```
In general, for consistency, your locustfile template should be saved in
`demos/applications/<application_name>/load_generation/load.pbtxt`.

# Creating the Docker Image
You'll first need to build the Locust wrapper binary that will be included in the our Docker image.
Run `bazel build //demos/load_generation:load_generation` in a _Linux_ environment,
not your Mac environment and move the generated binary to `demos/load_generation`. Ie in the docker
environment, you'd find it at

```
pixielabs/bazel-out/k8-fastbuild/bin/demos/load_generation/linux_amd64_stripped/load_generation
```

From the demos directory, run the following command to generate the Docker image:
```
docker build -f load_generation/Dockerfile -t <image-name> \
 --build-arg configDir=applications/<application-name>/load_generation .
```
You can push this image to our container registry using docker push.
Update the application's loadgenerator manifest with the new image tag and run the following to push the image to GKE.
```
kubectl create -f kubernetes_manifests/loadgenerator.yaml
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
