default['clang']               = {}
default['clang']['deb']        =
  'https://storage.googleapis.com/pl-infra-dev-artifacts/clang-9.0-pl5.deb'
default['clang']['deb_sha256'] =
  '6f261139154d4ffff9b3b5b2f049ab5e729df8d4fee1f55ffdb716a5d23a7e22'
default['clang']['version'] = "9.0-pl3"

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
default['shellcheck']                = {}

if node[:platform] == 'ubuntu'
  default['bazel']['download_path'] =
    'https://github.com/bazelbuild/bazel/releases/download/2.1.1/bazel-2.1.1-linux-x86_64'
  default['bazel']['sha256'] =
    '8684b902537b83ef37481b238bea8c5c323f1db6ba344b17398dff23a8f3563b'

  default['golang']['download_path'] =
    'https://dl.google.com/go/go1.13.linux-amd64.tar.gz'
  default['golang']['sha256'] =
    '68a2297eb099d1a76097905a2ce334e3155004ec08cdea85f24527be3c48e856'

  default['skaffold']['download_path'] =
    'https://storage.googleapis.com/pl-infra-dev-artifacts/skaffold/84eafe1c/skaffold-linux-amd64'
  default['skaffold']['sha256']        =
    '40b678384fb26c5b429160bf9d4b5cf60a619ce3f6b5d771335bde74df123024'

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
    'https://github.com/kubernetes-sigs/kustomize/releases/download/v3.2.0/kustomize_3.2.0_linux_amd64'
  default['kustomize']['sha256']        =
    '7db89e32575d81393d5d84f0dc6cbe444457e61ce71af06c6e6b7b6718299c22'

  default['sops']['download_path'] =
    'https://github.com/mozilla/sops/releases/download/3.3.1/sops-3.3.1.linux'
  default['sops']['sha256']        =
    '6eacdd01b68fd140eb71bbca233bea897cccb75dbf9e00a02e648b2f9a8a6939'

  default['shellcheck']['download_path'] =
    'https://storage.googleapis.com/shellcheck/shellcheck-v0.7.0.linux.x86_64.tar.xz'
  default['shellcheck']['sha256']        =
    '39c501aaca6aae3f3c7fc125b3c3af779ddbe4e67e4ebdc44c2ae5cba76c847f'
elsif node[:platform] == 'mac_os_x'
  default['bazel']['download_path'] =
    'https://github.com/bazelbuild/bazel/releases/download/2.1.1/bazel-2.1.1-darwin-x86_64'
  default['bazel']['sha256'] =
    '66dcbf562ce264e7f80dd20c82e2b3708a6e71c0bbd4368adccc40a8d1126cb3'

  default['golang']['download_path'] =
    'https://dl.google.com/go/go1.13.darwin-amd64.tar.gz'
  default['golang']['sha256'] =
    '234ebbba1fbed8474340f79059cfb3af2a0f8b531c4ff0785346e0710e4003dd'

  default['skaffold']['download_path'] =
    'https://storage.googleapis.com/skaffold/releases/v1.3.1/skaffold-darwin-amd64'
  default['skaffold']['sha256']        = 'e2f9eb8277e95db95384e2093afbb5bd571347ea065689d8cfd54bae2b301b9a'

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
    'https://github.com/kubernetes-sigs/kustomize/releases/download/v3.2.0/kustomize_3.2.0_darwin_amd64'
  default['kustomize']['sha256']        =
    'c7991a79470a52a95f1fac33f588b76f64e597ac64b54106e452f3a8f642c62e'

  default['sops']['download_path'] =
    'https://github.com/mozilla/sops/releases/download/3.3.1/sops-3.3.1.darwin'
  default['sops']['sha256']        =
    '09bb5920ae609bdf041b74843e2d8211a7059847b21729fadfbd3c3e33e67d26'

  default['shellcheck']['download_path'] =
    'https://storage.googleapis.com/shellcheck/shellcheck-v0.7.0.darwin.x86_64.tar.xz'
  default['shellcheck']['sha256']        =
    'c4edf1f04e53a35c39a7ef83598f2c50d36772e4cc942fb08a1114f9d48e5380'
end
