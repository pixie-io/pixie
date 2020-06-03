# Config files for deployment on K8s.

We use Kustomize to manage the generation of our config files
and Skaffold to manage deployments.

The directory structure is as follows:

```
  directory_per_deploy/   (vizier, pixie_cloud, stirling_wrapper)
    directory_per_env/    (base, dev, prod)
```
