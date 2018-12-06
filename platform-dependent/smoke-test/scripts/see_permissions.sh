echo "IAM/GCR"
gsutil iam get gs://artifacts.pl-shared.appspot.com
echo
echo "ACL/GCS"
gsutil acl get gs://pl-infra-shared/
