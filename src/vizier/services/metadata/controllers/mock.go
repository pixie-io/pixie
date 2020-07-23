package controllers

//go:generate mockgen -source=metadata_handler.go  -destination=mock/mock_metadata_store.gen.go
//go:generate mockgen -source=agent.go  -destination=mock/mock_agent.gen.go
//go:generate mockgen -source=probe.go -destination=mock/mock_probe_store.gen.go
