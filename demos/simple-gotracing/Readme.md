## Demo of dynamic Go tracing

This is a simple demo showing the ability of Pixie to add dynamic tracepoints into
Go binaries. This capability allows debugging Go binaries in production without the need
to the source code and recompile with addition log statements.

A simple view of this functionality is show in this image:

![Dynamic Logs](./dynamic_logs.png)

In a legacy systems a modify, compile, deploy cycle is required to get visibility into the binary
if the appropriate log lines are missing. Pixie allows
you to dynamically capture function arguments, latency, etc., without this slow
process. 

## Pre-reqs
Pixie needs to be installed on your K8s cluster. If it is not already installed then
please consult our [docs](http://https://docs.pixielabs.ai/).

## Running the demo
The demo is completely self-contained and will install a simple a go application under the
namespace px-demo-gotracing. The source of this application is in app.go. To deploy this application run:

```
# kubectl apply -f k8s_manifest.yaml
```

We are going to dynamic trace the `computeE` function in app.go to get started. This is a simple function
that tried to approximate the value of [eulers number](https://en.wikipedia.org/wiki/E_(mathematical_constant)) by
using a taylor series. The number of iterations of the expansion is specified by the `iters` query param to the HTTP handler. 
To see how this works we can get the external service address by running the following to get the external IP of the service:

```
# kubectl -n px-demo-gotracing get services -o wide

NAME            TYPE           CLUSTER-IP     EXTERNAL-IP   PORT(S)          AGE   SELECTOR
gotracing-svc   LoadBalancer   10.137.11.73   10.32.0.34    9090:31248/TCP   85m   app=gotracing
```

We can use `curl` to quickly access the api. The number of iterations is the query parameter `iters`.
```
# curl http://10.32.0.34:9090/e
e = 2.7183

# curl http://10.32.0.34:9090/e\?iters\=2
e = 2.0000

# curl http://10.32.0.34:9090/e\?iters\=200
e = 2.7183
```
 
As expected the accuracy of e approaches the expected value of `2.7183` as we 
increase the number of iterations. 

Let's say we want to quickly access the arguments to the `computeE`
function, and it's latency. We can use the provided `capture_args.pxl` script. As a first step we need to find 
the `UPID` of the process we want to trace. The `UPID` refers the _unique process id_, which is a process ID that
is globally unique in the entire cluster. In future versions of Pixie we will make this process easier. For now, we can
easily get the `UPID` by running the follow script:

```
# TODO() GET UPID Script
```  

We can then edit the `upid` variable in the `capture_args.pxl` script with this value from above.

```
[130] % px run -f capture_args.pxl
[0000]  INFO Pixie CLI
 ✔    Preparing schema
 ✔    Deploying compute_e_data
Table ID: output
  CLUSTERID    UPID  TIME   GOID   ITERATIONS
```

The result data will be empty since no requires are capture. Let's run the curl commands we have above and see what happens:

```
# px run -f capture_args.pxl
[0000]  INFO Pixie CLI
Table ID: output
  CLUSTERID                             UPID                                  TIME                       GOID    ITERATIONS
  f890689b-299c-43fd-8d2a-b0c528a58393  00000003-0024-844a-0000-000008caa618  2020-08-09T17:16:11-07:00  194529  100
  f890689b-299c-43fd-8d2a-b0c528a58393  00000003-0024-844a-0000-000008caa618  2020-08-09T17:16:14-07:00  194416  2
  f890689b-299c-43fd-8d2a-b0c528a58393  00000003-0024-844a-0000-000008caa618  2020-08-09T17:16:16-07:00  194531  200
``` 

There it is, we just capture all the arguments to the computeE function without changing the source code or redeploying it. We also found out
that the default number of iterations is a 100 without having to look through the source code. While this example is straight forward and simple
and hardly requires the use of dynamic logging to understand, we can easily see how this can be used to debug much more complicated scenarios. 


## Cleaning up
To delete the demo from the cluster just run: 

```
kubectl delete namespace px-demo-gotracing
```

## Modifying the demo
The demo can easily be modified by editing the app.go source file. After that you can simply create a new
docker image by running:

```
docker build . -t <image name>
```

Edit the image name in `k8s_manifest.yaml` to correspond to you new image and redeploy. 