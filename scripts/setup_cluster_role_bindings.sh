#!/bin/bash

# Setup cluster-role-bindings

# WARNING: This is very insecure!!!
# We are making the current user a cluster-admin.
# TODO(oazizi/philkuz): Change this once we understand RBAC better.
username=`whoami`
user_email=`git config user.email`
kubectl get clusterrolebinding ${username}-binding > /dev/null 2> /dev/null
if [ $? -ne 0 ]; then
  kubectl create clusterrolebinding ${username}-binding --clusterrole=cluster-admin --user=${user_email}
fi
