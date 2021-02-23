package publicvizierapipb

//go:generate mockgen -source=vizierapi.pb.go -destination=mock/vizier_mock.gen.go VizierService_ExecuteScriptServer
