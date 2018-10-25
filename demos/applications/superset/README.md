# Docker Container
Location: gcr.io/pl-dev-infra/demos/superset:0.28.1
This docker container was built from apache incubator-superset
https://github.com/apache/incubator-superset

# How to Run the Application on GKE
The k8s manifest files are included in the folder for the application:
`//pixielabs.ai/pixielabs/demos/applications/superset/*.yaml`

Run the following command after setting the kubectl context for a cluster

```kubectl create -f postgres-deployment.yaml,postgres-persistentvolumeclaim.yaml,
postgres-service.yaml,redis-deployment.yaml,redis-persistentvolumeclaim.yaml,
redis-service.yaml,superset-claim0-persistentvolumeclaim.yaml,superset-deployment.yaml,
superset-node-modules-persistentvolumeclaim.yaml,superset-service.yaml
```

# Create the admin user
You will need to log into the superset pod. First get a list of the pods using the following command:

```kubectl get pods```

Look for the pod with the superset prefix. Then execute the following command
to get a shell prompt in the container:

```kubectl exec -it superset-<some random chars> -- bash```

Now, you need to create the admin user.

```fabmanager --create-admin --app superset```

You will be prompted for the username, password, etc. Once you have set these,
the admin user will be created for the application.

# Accessing the application

The superset service has been exposed to only the Pixie Labs office IP.
You may not be able to access the service anywhere else. To get the external ip:

```kubectl get services superset
NAME       TYPE           CLUSTER-IP     EXTERNAL-IP     PORT(S)          AGE
superset   LoadBalancer   10.7.249.138   35.224.177.88   8088:31141/TCP   18m
```

Load the external ip address in a browser and specify the port as 8088.
http://35.224.177.88:8088
You should be able to log in as the admin user and browse the application.
