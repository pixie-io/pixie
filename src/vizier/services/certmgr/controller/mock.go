package controller

//go:generate mockgen -source=server.go  -destination=mock/mock_k8s_api.gen.go K8sAPI
