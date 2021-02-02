package controllers

//go:generate mockgen -source=metadata_handler.go  -destination=mock/mock_metadata_handler.gen.go
//go:generate mockgen -source=agent.go  -destination=mock/mock_agent.gen.go
//go:generate mockgen -source=tracepoint.go -destination=mock/mock_tracepoint.gen.go
