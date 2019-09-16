default['clang']               = {}
default['clang']['deb']        =
  'https://storage.googleapis.com/pl-infra-dev-artifacts/clang-8.0-pl1.deb'
default['clang']['deb_sha256'] =
  'fe4c6d291de55a5963a4cf328ffbc7f6416082396a2014cfe827feae02a17f57'
default['clang']['version'] = "8.0-pl1"

default['gperftools']               = {}
default['gperftools']['deb']        =
  'https://storage.googleapis.com/pl-infra-dev-artifacts/gperftools-pixie-2.7-pl2.deb'
default['gperftools']['deb_sha256'] =
  'f43a343a6eae52dfd9ef1a3e3a9fe14347587d05fa7397f9444b473a4a65e959'
default['gperftools']['version'] = "2.7-pl2"


default['skaffold']                  = {}
default['kubectl']                   = {}
default['bazel']                     = {}
default['golang']                    = {}
default['sops']                      = {}

if node[:platform] == 'ubuntu'
  default['bazel']['download_path'] =
    'https://github.com/bazelbuild/bazel/releases/download/0.29.1/bazel-0.29.1-linux-x86_64'
  default['bazel']['sha256'] =
    'da3031d811f42f6208d24a87984b5b07e1c75afede184cad86eb02bef6c3b9b0'

  default['golang']['download_path'] =
    'https://dl.google.com/go/go1.11.10.linux-amd64.tar.gz'
  default['golang']['sha256'] =
    'aefaa228b68641e266d1f23f1d95dba33f17552ba132878b65bb798ffa37e6d0'

  default['skaffold']['download_path'] =
    'https://storage.googleapis.com/skaffold/releases/v0.34.0/skaffold-linux-amd64'
  default['skaffold']['sha256']        =
    '5867f2e92c3694da3d7ef2d9240416d693557af5caf56b13bfc1533c7940b341'

  default['kubectl']['download_path'] =
    'https://storage.googleapis.com/kubernetes-release/release/v1.14.6/bin/linux/amd64/kubectl'
  default['kubectl']['sha256']        = '5f8e8d8de929f64b8f779d0428854285e1a1c53a02cc2ad6b1ce5d32eefad25c'

  default['minikube']['download_path'] =
    'https://storage.googleapis.com/minikube/releases/v1.0.0/minikube-linux-amd64'
  default['minikube']['sha256']        =
    'a315869f81aae782ecc6ff2a6de4d0ab3a17ca1840d1d8e6eea050a8dd05907f'

  default['nodejs']['download_path'] = 'https://nodejs.org/dist/v10.13.0/node-v10.13.0-linux-x64.tar.gz'
  default['nodejs']['sha256']        = 'b4b5d8f73148dcf277df413bb16827be476f4fa117cbbec2aaabc8cc0a8588e1'

  default['packer']['download_path'] = 'https://releases.hashicorp.com/packer/1.3.2/packer_1.3.2_linux_amd64.zip'
  default['packer']['sha256']        = '5e51808299135fee7a2e664b09f401b5712b5ef18bd4bad5bc50f4dcd8b149a1'

  default['kustomize']['download_path'] =
    'https://github.com/kubernetes-sigs/kustomize/releases/download/v3.0.0/kustomize_3.0.0_linux_amd64'
  default['kustomize']['sha256']        =
    'ef0dbeca85c419891ad0e12f1f9df649b02ceb01517fa9aea0297ef14e400c7a'

  default['sops']['download_path'] =
    'https://github.com/mozilla/sops/releases/download/3.3.1/sops-3.3.1.linux'
  default['sops']['sha256']        =
    '6eacdd01b68fd140eb71bbca233bea897cccb75dbf9e00a02e648b2f9a8a6939'
elsif node[:platform] == 'mac_os_x'
  default['bazel']['download_path'] =
    'https://github.com/bazelbuild/bazel/releases/download/0.29.1/bazel-0.29.1-darwin-x86_64'
  default['bazel']['sha256'] =
    '34daae4caafbdb0952415ed6f97f47f03df84df9af146e9eb910ba65c073efdd'

  default['golang']['download_path'] =
    'https://dl.google.com/go/go1.11.10.darwin-amd64.tar.gz'
  default['golang']['sha256'] =
    '194d7ce2b88a791147be64860f21bac8eeec8f372c9e9caab6c72c3bd525a039'

  default['skaffold']['download_path'] =
    'https://storage.googleapis.com/skaffold/releases/v0.34.0/skaffold-darwin-amd64'
  default['skaffold']['sha256']        = '71cf275a40c0c2763b66e0c975ac781d65202b1e355f3447b839439dfd01b27b'

  default['kubectl']['download_path'] =
    'https://storage.googleapis.com/kubernetes-release/release/v1.14.6/bin/darwin/amd64/kubectl'
  default['kubectl']['sha256']        = 'de42dd22f67c135b749c75f389c70084c3fe840e3d89a03804edd255ac6ee829'

  default['minikube']['download_path'] =
    'https://storage.googleapis.com/minikube/releases/v1.0.0/minikube-darwin-amd64'
  default['minikube']['sha256']        = '865bd3a13c1ad3b7732b2bea35b26fef150f2b3cbfc257c5d1835527d1b331e9'

  default['nodejs']['download_path'] = 'https://nodejs.org/dist/v10.13.0/node-v10.13.0-darwin-x64.tar.gz'
  default['nodejs']['sha256']        = '815a5d18516934a3963ace9f0574f7d41f0c0ce9186a19be3d89e039e57598c5'

  default['packer']['download_path'] = 'https://releases.hashicorp.com/packer/1.3.2/packer_1.3.2_darwin_amd64.zip'
  default['packer']['sha256']        = '1c2433239d801b017def8e66bbff4be3e7700b70248261b0abff2cd9c980bf5b'

  default['kustomize']['download_path'] =
    'https://github.com/kubernetes-sigs/kustomize/releases/download/v3.0.0/kustomize_3.0.0_darwin_amd64'
  default['kustomize']['sha256']        =
    '58bf0cf1fe6839a1463120ced1eae385423efa6437539eb491650db5089c60b9'

  default['sops']['download_path'] =
    'https://github.com/mozilla/sops/releases/download/3.3.1/sops-3.3.1.darwin'
  default['sops']['sha256']        =
    '09bb5920ae609bdf041b74843e2d8211a7059847b21729fadfbd3c3e33e67d26'
end
