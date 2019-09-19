---
title: "Configuration"
metaTitle: "Administration | Pixie"
metaDescription: "How to Install ..."
---

Pixie's deployment experience is designed to be easy by minimizing the need for insturmenation or configuration. However, we'd like to empower Admins with a powerful CLI to configure and manage the Pixie system to best fit their needs. 


#### Pixie CLI Commands:

```
Available Commands:
    delete        Deletes Pixie on the current K8s cluster
    deploy        Deploys Pixie on the current K8s cluster
    help          Help about any command
    install-certs Generates the proper server and client certs
    version       Print the version number of the cli

Flags:
    --bit_size int              Size in bits of the generated key (optional) (default 4096)
    --ca_cert string            Path to CA cert (optional)
    --ca_key string             Path to CA key (optional)
    --cert_path string          Directory to save certs in
    --check                     Check whether the cluster can run Pixie
    --clobber_namespace         Whether to delete all dependencies in the cluster
    --cluster_id string         The ID of the cluster
    --extract_yaml string       Directory to extract the Pixie yamls to
    -h, --help                  Help for pixie-admin
    --namespace string          The namespace to install certs or Pixie K8s secrets to (default "pl")
    --registration_key string   The registration key to use for this cluster
    --secret_name string        The name of the secret used to access the Pixie images (default "pl-image-secret")
    --use_version string        Pixie version to deploy
```

#### Recommendations

- **Configuring Memory Allocation:** Extract and manually update Pixie YAML files using the  `--extract_yaml string` flag. 
