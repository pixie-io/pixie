package k8smeta

//go:generate genny -in=k8s_metadata_utils.tmpl -out k8s_metadata_utils.gen.go gen "ReplacedResource=Pod,Service,Namespace,Endpoints,Node"
