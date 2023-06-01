- For each server, run the following only once:
```
./setup_raid.sh <SERVER_IP>
```
- Install k3sup (this installs k3sup to `~/bin` if that's not in your path choose somewhere that is)
```
curl -sLS https://get.k3sup.dev | sh
install k3sup ~/bin
```
- Make sure your ssh key has access to all of the machines you are going to use.
- Provision an EIP. Make sure the EIP is tagged with a unique identifier.
- Run
```
EQUINIX_API_KEY=<api_key> \
EQUINIX_PROJECT_ID=<project_id> \
EQUINIX_METRO=<equinix metro of machines, eg. la> \
EIP=<eip> \
EIP_TAG=<eip_tag> \
SERVER_IPS=<comma separated list of IPS> \
AGENT_IPS=<optional comma separated list of IPS> \
./setup_k3s.sh <path/to/output/kubeconfig>
```


To teardown the cluster:
- Make sure your ssh key has access to all of the machines you are going to use.
- Run
```
EIP=<eip> \
SERVER_IPS=<comma separated list of IPS> \
AGENT_IPS=<optional comma separated list of IPS> \
./teardown.sh
```
