This directory contains a simple client that communicates with the `productcatalogservice` from `online-boutique`
Link to `online-boutique`: https://github.com/GoogleCloudPlatform/microservices-demo.

It is intended for use in testing code inside Stirling.

To run it stand-alone, you can use these steps:

```
# Run the server
docker run -e DISABLE_PROFILER=1 -e DISABLE_TRACING=1 -p 3550:3550 gcr.io/google-samples/microservices-demo/productcatalogservice:v0.2.0

# Run the client
bazel run //src/stirling/testing/demo_apps/hipster_shop/productcatalogservice_client:productcatalogservice_client_image
```
