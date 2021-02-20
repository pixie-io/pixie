package controllers

//go:generate mockgen -source=agent.go  -destination=mock/mock_agent.gen.go AgentManager
//go:generate mockgen -source=tracepoint.go -destination=mock/mock_tracepoint.gen.go TracepointStore
