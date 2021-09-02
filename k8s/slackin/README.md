## Slackin
Slackin is our slack signup helper. More info [here](https://github.com/pixie-io/slackin).


## Deployment instructions

The deploy the app run the following from PIXIE_ROOT:

```bash
kubectl create namespace slackin
sops -d credentials/k8s/slackin/slackin_secrets.yaml | kubectl apply -f -
sops -d credentials/k8s/slackin/slackin_config.yaml | kubectl apply -f -
kustomize build k8s/slackin | kubectl apply -f -
```
