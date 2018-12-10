export EMAIL=$1

gsutil iam ch -d user:$EMAIL:objectViewer gs://artifacts.pl-shared.appspot.com
gsutil acl ch -d $EMAIL gs://pl-infra-shared
