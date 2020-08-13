default['clang']               = {}
default['clang']['deb']        =
  'https://storage.googleapis.com/pl-infra-dev-artifacts/clang-10.0-pl1.deb'
default['clang']['deb_sha256'] =
  'abd6e0e32379fb83b359ddeb38ecd35fffb8dbf17e8450312c66bc86037d01a8'
default['clang']['version'] = "10.0-pl1"

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
default['sentry']                    = {}
default['prototool']                 = {}
default['yq']                        = {}

if node[:platform] == 'ubuntu'
  default['bazel']['download_path'] =
    'https://github.com/bazelbuild/bazel/releases/download/3.4.1/bazel-3.4.1-linux-x86_64'
  default['bazel']['sha256'] =
    '1a64c807716e10c872f1618852d95f4893d81667fe6e691ef696489103c9b460'

  default['golang']['download_path'] =
    'https://dl.google.com/go/go1.13.8.linux-amd64.tar.gz'
  default['golang']['sha256'] =
    '0567734d558aef19112f2b2873caa0c600f1b4a5827930eb5a7f35235219e9d8'

  default['skaffold']['download_path'] =
    'https://storage.googleapis.com/skaffold/releases/v1.12.1/skaffold-linux-amd64'
  default['skaffold']['sha256']        =
    'e96db5103448663d349072c515ddae33bdf05727689a9a3460f3f36a41a94b8e'

  default['kubectl']['download_path'] =
    'https://storage.googleapis.com/kubernetes-release/release/v1.14.6/bin/linux/amd64/kubectl'
  default['kubectl']['sha256']        = '5f8e8d8de929f64b8f779d0428854285e1a1c53a02cc2ad6b1ce5d32eefad25c'

  default['minikube']['download_path'] =
    'https://github.com/kubernetes/minikube/releases/download/v1.9.2/minikube-linux-amd64'
  default['minikube']['sha256']        =
    '3121f933bf8d608befb24628a045ce536658738c14618504ba46c92e656ea6b5'

  default['nodejs']['download_path'] = 'https://nodejs.org/dist/v12.16.1/node-v12.16.1-linux-x64.tar.gz'
  default['nodejs']['sha256']        = 'b2d9787da97d6c0d5cbf24c69fdbbf376b19089f921432c5a61aa323bc070bea'

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
    'https://github.com/koalaman/shellcheck/releases/download/v0.7.0/shellcheck-v0.7.0.linux.x86_64.tar.xz'
  default['shellcheck']['sha256']        =
    '39c501aaca6aae3f3c7fc125b3c3af779ddbe4e67e4ebdc44c2ae5cba76c847f'

  default['sentry']['download_path'] =
    'https://github.com/getsentry/sentry-cli/releases/download/1.52.0/sentry-cli-Linux-x86_64'
  default['sentry']['sha256']        =
    'd6aeb45efbcdd3ec780f714b5082046ea1db31ff60ed0fc39916bbc8b6d708be'

  default['prototool']['download_path'] =
    'https://github.com/uber/prototool/releases/download/v1.10.0/prototool-Linux-x86_64'
  default['prototool']['sha256']        =
    '2247ff34ad31fa7d9433b3310879190d1ab63b2ddbd58257d24c267f53ef64e6'

  default['yq']['download_path'] =
    'https://github.com/mikefarah/yq/releases/download/3.2.1/yq_linux_amd64'
  default['yq']['sha256']        =
    '11a830ffb72aad0eaa7640ef69637068f36469be4f68a93da822fbe454e998f8'

elsif node[:platform] == 'mac_os_x'
  default['bazel']['download_path'] =
    'https://github.com/bazelbuild/bazel/releases/download/3.4.1/bazel-3.4.1-darwin-x86_64'
  default['bazel']['sha256'] =
    '23ea93e5ef9f381babd7f658bbbf12d366510f0a719e816fea90bcbe9737aef2'

  default['golang']['download_path'] =
    'https://dl.google.com/go/go1.13.8.darwin-amd64.tar.gz'
  default['golang']['sha256'] =
    'e7bad54950e1d18c716ac9202b5406e7d4aca9aa4ca9e334a9742f75c2167a9c'

  default['skaffold']['download_path'] =
    'https://storage.googleapis.com/skaffold/releases/v1.12.1/skaffold-darwin-amd64'
  default['skaffold']['sha256']        = '6b2bd0ae47dda96d64661136222622d97aaa9bd020b67f77fb744f944cd47ae5'

  default['kubectl']['download_path'] =
    'https://storage.googleapis.com/kubernetes-release/release/v1.14.6/bin/darwin/amd64/kubectl'
  default['kubectl']['sha256']        = 'de42dd22f67c135b749c75f389c70084c3fe840e3d89a03804edd255ac6ee829'

  default['minikube']['download_path'] =
    'https://github.com/kubernetes/minikube/releases/download/v1.9.2/minikube-darwin-amd64'
  default['minikube']['sha256']        = 'f27016246850b3145e1509e98f7ed060fd9575ac4d455c7bdc15277734372e85'

  default['nodejs']['download_path'] = 'https://nodejs.org/dist/v12.16.1/node-v12.16.1-darwin-x64.tar.gz'
  default['nodejs']['sha256']        = '34895bce210ca4b3cf19cd480e6563588880dd7f5d798f3782e3650580d35920'

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
    'https://github.com/koalaman/shellcheck/releases/download/v0.7.0/shellcheck-v0.7.0.darwin.x86_64.tar.xz'
  default['shellcheck']['sha256']        =
    'c4edf1f04e53a35c39a7ef83598f2c50d36772e4cc942fb08a1114f9d48e5380'

  default['sentry']['download_path'] =
    'https://github.com/getsentry/sentry-cli/releases/download/1.52.0/sentry-cli-Darwin-x86_64'
  default['sentry']['sha256']        =
    '97c9bafbcf87bd7dea4f1069fe18f8e8265de6f7eab20c62ca9299e0fa8c2af6'

  default['prototool']['download_path'] =
    'https://github.com/uber/prototool/releases/download/v1.10.0/prototool-Darwin-x86_64'
  default['prototool']['sha256']        =
    '5ca2a19f1cb04bc5059bb07e14d565231246e623f7523fafe9389b463addf645'

  default['yq']['download_path'] =
    'https://github.com/mikefarah/yq/releases/download/3.2.1/yq_darwin_amd64'
  default['yq']['sha256']        =
    '116f74a384d0b4fa31a58dd01cfcdeffa6fcd21c066de223cbb0ebc042a8bc28'
end
