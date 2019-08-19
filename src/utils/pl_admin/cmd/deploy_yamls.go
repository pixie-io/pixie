package cmd

//go:generate mkdir -p /tmp/vizier_yamls
//go:generate kustomize build ../../../../k8s/vizier/base -o /tmp/vizier_yamls/vizier.yaml
//go:generate kustomize build ../../../../k8s/vizier_deps/base/etcd -o /tmp/vizier_yamls/etcd.yaml
//go:generate kustomize build ../../../../k8s/vizier_deps/base/nats -o /tmp/vizier_yamls/nats.yaml
//go:generate go-bindata -pkg=cmd -o=bindata.gen.go /tmp/vizier_yamls
