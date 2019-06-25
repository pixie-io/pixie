default['bcc']               = {}
default['bcc']['deb']        =
  'https://storage.googleapis.com/pl-infra-dev-artifacts/bcc-pixie-1.1.deb'
default['bcc']['deb_sha256'] =
  '9e4846adc6da8f042a3a846810145c2025cb08a1d412be91cd34c40912566e56'
default['bcc']['version'] = "1.1"

default['clang']               = {}
default['clang']['deb']        =
  'https://storage.googleapis.com/pl-infra-dev-artifacts/clang-7.0-pl8.deb'
default['clang']['deb_sha256'] =
  'bddb994b2851210ba70c202b67a92f67f710b9fc878fe1af4e77a2be0f75833c'
default['clang']['version'] = "7.0-pl8"

default['gperftools']               = {}
default['gperftools']['deb']        =
  'https://storage.googleapis.com/pl-infra-dev-artifacts/gperftools-pixie-2.7-pl1.deb'
default['gperftools']['deb_sha256'] =
  '8b215251c3514504f04fbf4673edd26aa7c37663f4109bad6f9fa0f2adff427e'
default['gperftools']['version'] = "2.7-pl1"


default['skaffold']                  = {}
default['bazel']                     = {}
default['golang']                    = {}

if node[:platform] == 'ubuntu'
  default['bazel']['deb']        =
    'https://github.com/bazelbuild/bazel/releases/download/0.27.0/bazel_0.27.0-linux-x86_64.deb'
  default['bazel']['deb_sha256'] =
    '489d2ea3f4dc0822bd049df8262ee5c9efa8eb22a55596bbb100c0b3e80a40e7'
  default['bazel']['version'] = "0.27.0"

  default['golang']['download_path'] =
    'https://dl.google.com/go/go1.11.10.linux-amd64.tar.gz'
  default['golang']['sha256'] =
    'aefaa228b68641e266d1f23f1d95dba33f17552ba132878b65bb798ffa37e6d0'

  default['skaffold']['download_path'] =
    'https://storage.googleapis.com/skaffold/releases/v0.27.0/skaffold-linux-amd64'
  default['skaffold']['sha256']        =
    'd40e9fb9a9a62d962537a0d02ca5966b7c63541f7c769e19c02848653393a941'

  default['minikube']['download_path'] =
    'https://storage.googleapis.com/minikube/releases/v1.0.0/minikube-linux-amd64'
  default['minikube']['sha256']        =
    'a315869f81aae782ecc6ff2a6de4d0ab3a17ca1840d1d8e6eea050a8dd05907f'

  default['nodejs']['download_path'] = 'https://nodejs.org/dist/v10.13.0/node-v10.13.0-linux-x64.tar.gz'
  default['nodejs']['sha256']        = 'b4b5d8f73148dcf277df413bb16827be476f4fa117cbbec2aaabc8cc0a8588e1'

  default['packer']['download_path'] = 'https://releases.hashicorp.com/packer/1.3.2/packer_1.3.2_linux_amd64.zip'
  default['packer']['sha256']        = '5e51808299135fee7a2e664b09f401b5712b5ef18bd4bad5bc50f4dcd8b149a1'
elsif node[:platform] == 'mac_os_x'
  default['bazel']['download_path'] =
    'https://github.com/bazelbuild/bazel/releases/download/0.27.0/bazel-0.27.0-darwin-x86_64'
  default['bazel']['sha256'] =
    '8c222c7c8d6814341c556649ddfb6be2cf58ae483888a7b6675f96523e1107d0'

  default['golang']['download_path'] =
    'https://dl.google.com/go/go1.11.10.darwin-amd64.tar.gz'
  default['golang']['sha256'] =
    '194d7ce2b88a791147be64860f21bac8eeec8f372c9e9caab6c72c3bd525a039'

  default['skaffold']['download_path'] =
    'https://storage.googleapis.com/skaffold/releases/v0.27.0/skaffold-darwin-amd64'
  default['skaffold']['sha256']        = 'bc20412b585106ed742ca7fa1d43e117597f85e5c162b69e6ce64cd4d8278d5e'

  default['minikube']['download_path'] =
    'https://storage.googleapis.com/minikube/releases/v1.0.0/minikube-darwin-amd64'
  default['minikube']['sha256']        = '865bd3a13c1ad3b7732b2bea35b26fef150f2b3cbfc257c5d1835527d1b331e9'

  default['nodejs']['download_path'] = 'https://nodejs.org/dist/v10.13.0/node-v10.13.0-darwin-x64.tar.gz'
  default['nodejs']['sha256']        = '815a5d18516934a3963ace9f0574f7d41f0c0ce9186a19be3d89e039e57598c5'

  default['packer']['download_path'] = 'https://releases.hashicorp.com/packer/1.3.2/packer_1.3.2_darwin_amd64.zip'
  default['packer']['sha256']        = '1c2433239d801b017def8e66bbff4be3e7700b70248261b0abff2cd9c980bf5b'
end
