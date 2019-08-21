package vzconnpb

//go:generate sh -c "mockgen pixielabs.ai/pixielabs/src/cloud/vzconn/vzconnpb VZConnServiceServer,VZConnServiceClient,VZConnService_CloudConnectServer > mock/service_mock.gen.go"
