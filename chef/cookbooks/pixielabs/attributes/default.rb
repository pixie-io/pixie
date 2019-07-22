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
default['bazel']                     = {}
default['golang']                    = {}

if node[:platform] == 'ubuntu'
  default['bazel']['deb']        =
    'https://github.com/bazelbuild/bazel/releases/download/0.26.1/bazel_0.26.1-linux-x86_64.deb'
  default['bazel']['deb_sha256'] =
    'c0b2b676ca7cc071a98f969aefb4a9d4b7db1858b7340d9db6f8076179e776cd'
  default['bazel']['version'] = "0.26.1"

  default['golang']['download_path'] =
    'https://dl.google.com/go/go1.11.10.linux-amd64.tar.gz'
  default['golang']['sha256'] =
    'aefaa228b68641e266d1f23f1d95dba33f17552ba132878b65bb798ffa37e6d0'

  default['skaffold']['download_path'] =
    'https://storage.googleapis.com/skaffold/releases/v0.34.0/skaffold-linux-amd64'
  default['skaffold']['sha256']        =
    '5867f2e92c3694da3d7ef2d9240416d693557af5caf56b13bfc1533c7940b341'

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
elsif node[:platform] == 'mac_os_x'
  default['bazel']['download_path'] =
    'https://github.com/bazelbuild/bazel/releases/download/0.26.1/bazel-0.26.1-darwin-x86_64'
  default['bazel']['sha256'] =
    '9b416e0c9bde5fd59264eacc35d518d2465b5591fbaf3656e386970c045d4747'

  default['golang']['download_path'] =
    'https://dl.google.com/go/go1.11.10.darwin-amd64.tar.gz'
  default['golang']['sha256'] =
    '194d7ce2b88a791147be64860f21bac8eeec8f372c9e9caab6c72c3bd525a039'

  default['skaffold']['download_path'] =
    'https://storage.googleapis.com/skaffold/releases/v0.34.0/skaffold-darwin-amd64'
  default['skaffold']['sha256']        = '71cf275a40c0c2763b66e0c975ac781d65202b1e355f3447b839439dfd01b27b'

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
end
