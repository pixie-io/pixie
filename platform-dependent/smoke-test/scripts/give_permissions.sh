export EMAIL=$1
gsutil iam ch user:$EMAIL:objectViewer gs://artifacts.pl-shared.appspot.com
gsutil acl ch -u $EMAIL:W gs://pl-infra-shared
