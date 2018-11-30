default['bazel']               = {}
default['bazel']['deb']        =
  'https://github.com/bazelbuild/bazel/releases/download/0.19.2/bazel_0.19.2-linux-x86_64.deb'
default['bazel']['deb_sha256'] =
  '5d22686faa59a5c0bba65c593c3f955a871f2712d306dd607f284cabd2b32c1f'

default['bcc']               = {}
default['bcc']['deb']        =
  'https://storage.googleapis.com/pl-infra-dev-artifacts/bcc-pixie-1.0.deb'
default['bcc']['deb_sha256'] =
  'd2a95ea6dae3758b26056147257adb3c851498fd1a63fc0f318c90722545ea36'

default['skaffold']                  = {}

if node[:platform] == 'ubuntu'
  default['skaffold']['download_path'] =
    'https://github.com/GoogleContainerTools/skaffold/releases/download/v0.16.0/skaffold-linux-amd64'
  default['skaffold']['sha256']        =
    '6f975cdc821617c06d9192c74eb6255769c66536d95d20ea8e53f8da97d40af0'

  default['nodejs']['download_path'] = 'https://nodejs.org/dist/v10.13.0/node-v10.13.0-linux-x64.tar.gz'
  default['nodejs']['sha256']        = 'b4b5d8f73148dcf277df413bb16827be476f4fa117cbbec2aaabc8cc0a8588e1'

  default['packer']['download_path'] = 'https://releases.hashicorp.com/packer/1.3.2/packer_1.3.2_linux_amd64.zip'
  default['packer']['sha256']        = '5e51808299135fee7a2e664b09f401b5712b5ef18bd4bad5bc50f4dcd8b149a1'
elsif node[:platform] == 'mac_os_x'
  default['skaffold']['download_path'] =
    'https://github.com/GoogleContainerTools/skaffold/releases/download/v0.16.0/skaffold-darwin-amd64'
  default['skaffold']['sha256']        = '95c431458586457b1691d52a4d5293510e1e421a7625a459da0e220144ef197b'

  default['nodejs']['download_path'] = 'https://nodejs.org/dist/v10.13.0/node-v10.13.0-darwin-x64.tar.gz'
  default['nodejs']['sha256']        = '815a5d18516934a3963ace9f0574f7d41f0c0ce9186a19be3d89e039e57598c5'

  default['packer']['download_path'] = 'https://releases.hashicorp.com/packer/1.3.2/packer_1.3.2_darwin_amd64.zip'
  default['packer']['sha256']        = '1c2433239d801b017def8e66bbff4be3e7700b70248261b0abff2cd9c980bf5b'
end
