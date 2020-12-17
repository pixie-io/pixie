package controllers

//go:generate mockgen -source=server.go  -destination=mock/mock_apikeymgr.gen.go
//go:generate mockgen -source=auth0.go -destination=mock/auth0_mock.gen.go
