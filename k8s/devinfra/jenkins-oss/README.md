Setup Helm:
  helm repo add jenkins https://charts.jenkins.io
  helm repo update

To deploy Jenkins run the following:
  helm upgrade cd-jenkins -f values.yaml jenkins/jenkins --wait


To get the password:
  echo Password: $(kubectl get secret --namespace jenkins-oss cd-jenkins -o jsonpath="{.data.jenkins-admin-password}" | base64 --decode)
