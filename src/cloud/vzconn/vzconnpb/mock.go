package vzconnpb

//go:generate mockgen -source=service.pb.go -destination=mock/service_mock.gen.go VZConnServiceServer,VZConnServiceClient,VZConnService_NATSBridgeServer,VZConnService_NATSBridgeClient
