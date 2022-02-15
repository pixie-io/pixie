# Containerized Build Rules for auxilary binaries

## pl_aux_go_binary

A rule for building auxiliary go binaries used in tests.
These builds are performed in a container, decoupling them from our go toolchain.
This lets us control the go version with which to build the auxiliary test binaries, so we can ensure Stirling works on different versions of Go.
It also provides more determinism in our tests (e.g. less churn on go toolchain upgrades).

### Usage notes

The rule will setup a alpine based golang container (see `base` param) and copy over all the files listed in the `files` arg into the source folder. If any more files in a specific directory structure are needed, the `extra_layers` arg can be used to specify a dict of `subfolder_name: subfolder_files`  mapping. The rule will create extra container layers to nest these files into the subfolder as needed. This is particularly useful for including generated proto files which are in a separate package and hence must be in a separate folder.

The rule expects the files copied over to also include the `go.mod` and `go.sum` for any dependencies your program might need. The easiest way to generate these is to copy the build files into a temp dir on your machine and run `go mod init` and `go get`. Be careful to remove the module name and go version from the `go.mod` file since both of those are dependent on the target name which is usually dependent on the version of golang being used. The same steps are necessary when updating any imports in any of the source files.
