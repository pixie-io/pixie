package authpb

//go:generate mockgen -source=auth.pb.go -destination=mock/auth_mock.gen.go AuthServiceClient,APIKeyServiceClient
