#!/bin/bash -e

DB="pl_prod"

usage() {
    echo "Usage: $0 [-p] [-s]"
    echo " -p : Log into the prod db"
    echo " -s : Log into the staging db"
}

if [ $# -gt 1 ]; then
usage
exit
fi

while test $# -gt 0; do
  case "$1" in
    -p) DB="pl_prod"
        shift
        ;;
    -s) DB="pl_staging"
        shift
        ;;
    *)  usage ;;
  esac
done


# Running this script requires access to the "prod-ro" (prod-readonly) namespace in the prod cluster.
POD_NAME=$(kubectl get pod --namespace prod-ro \
    --selector="name=db-reader" --output jsonpath='{.items[0].metadata.name}')

kubectl exec -it "$POD_NAME" -n prod-ro -- bash -c \
"psql postgresql://$(kubectl get secret pl-db-ro-secrets -n prod-ro -o json | \
jq -r '.data."PL_POSTGRES_USERNAME"'  | base64 --decode):$(kubectl get secret pl-db-ro-secrets  -n prod-ro  -o json | \
jq -r '.data."PL_POSTGRES_PASSWORD"'  | base64 --decode)@localhost:5432/$DB"
