package vzconnpb

//go:generate sh -c "mockgen pixielabs.ai/pixielabs/src/cloud/vzconn/vzconnpb VZConnServiceServer,VZConnServiceClient,VZConnService_CloudConnectServer,VZConnService_CloudConnectClient > mock/service_mock.gen.go"
