# Stirling Kubernetes Deployment Scripts

This directory contains scripts to deploy `stirling_wrapper` standalone on to a kubernetes cluster.

The primary script is `run_on_k8s.sh`.

When on GKE, you can simply run this script. On other clusters, you may need to set up image pull secrets, as described next.

## Image pull secrets

If running on a cluster other than GKE, you may require image pull secrets to access GCR.

To set this up, obtain the appropriate secret from:
`https://console.cloud.google.com/apis/credentials?project=pl-dev-infra` under the service account `k8s-gcr-auth-ro@pl-dev-infra.iam.gserviceaccount.com`

Download the key, and rename the file to `image-pull-secrets.json`.
The scripts in this directory will then install the image pull secrets automatically.

