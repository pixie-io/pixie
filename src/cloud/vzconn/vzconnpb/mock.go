package vzconnpb

//go:generate sh -c "mockgen pixielabs.ai/pixielabs/src/cloud/vzconn/vzconnpb VZConnServiceServer,VZConnServiceClient,VZConnService_NATSBridgeServer,VZConnService_NATSBridgeClient > mock/service_mock.gen.go"
