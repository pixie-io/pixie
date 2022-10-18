# Notes to deploy prow.

You can follow the setup guide over [here](https://github.com/kubernetes/test-infra/blob/master/prow/getting_started_deploy.md)

We use tackle with the following command:
`tackle --repo pixie-io/pixie -github-token-path $(sops -d <api_key_file>) -starter prow-setup-starter.yaml`
