# Demo app deployment instructions

For sockshop and hipster-shop, you should use the following script to deploy the application onto your cluster:

```
<ToT>/scripts/deploy_demo_apps.sh
```

There is also an option (`-l`) to deploy the load generator in case it exists.

## Manual deploy

Some apps have not been integrated into the automated script yet. In these cases, or in cases of debug,
one may want to deploy the kubernetes yamls directly with kubectl.

To deploy manually, you generally want to apply the manifest files in the kubernetes_manifests directory, similar to the following:

```
kubectl apply -f <app>/kubernetes_manifests
```

Notes

 * You may need to create the application namespace first, or run the kubectl command multiple times to make sure the namespace is created first.

## What to Do After Deployment

Run `kubectl get services` to find the front-end service for your app. You can then visit that website from your browser.
