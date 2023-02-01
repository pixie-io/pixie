# Stirling Kubernetes Deployment Scripts

This directory contains scripts to deploy `stirling_wrapper` standalone on to a kubernetes cluster.

The primary script is `run_stirling_wrapper_on_k8s.sh`.

When on GKE, you can simply run this script. On other clusters, you may need to set up image pull secrets, as described next.

## Setting up image pull secrets

The kubernetes deployments in `run_stirling_wrapper_on_k8s.sh` require a secret to pull the stirling container images. The required image-pull-secret is in our repo, but is encrypted. We use `sops` to decrypt the secret and install it into k8s.

Our scripts automatically handle the process for you, but `gcloud auth` needs to be properly setup for `sops` to be able to decrypt the secret. If you have not run the `gcloud auth` command on your machine before, `sops`  will complain about not being able to decrypt the json file. If you get this error, try the following:

1. `gcloud auth --no-launch-browser application-default login`
2. copy/paste the link into your local browser
3. copy/paste the verification code back onto the `gcloud auth` prompt.
