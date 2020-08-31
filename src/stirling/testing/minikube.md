# Minikube

## Running stirling_wrapper container on Minikube cluster

* Install and configure minikube with KVM driver, follow instructions at
  https://minikube.sigs.k8s.io/docs/drivers/kvm2/
* Then launch minikube cluster with `minikube start --driver=kvm2`
* Check that minikube cluster is running with:

  ```
  kubectl config use-context minikube
  kubectl get pods --all-namespaces
  ```

  You should see PODs in the `kube-system` namespace.
* Then launch `stirling_wrapper`: `src/stirling/k8s/run_on_k8s.sh 100`
  The argument to the script is the runtime given to the stirling_wrapper container.
* After the script exists, you can find the logs under `src/stirling/k8s/logs`.
