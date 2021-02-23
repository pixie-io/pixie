package cloudapipb

//go:generate mockgen -source=cloudapi.pb.go -destination=mock/cloudapi_mock.gen.go ArtifactTrackerServer,VizierClusterInfoServer,VizierDeploymentKeyManagerServer,ScriptMgrServer,AutocompleteServiceServer,APIKeyManagerServer
