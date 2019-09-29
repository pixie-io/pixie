package controller

import (
	"context"
	"errors"

	"pixielabs.ai/pixielabs/src/vizier/services/certmgr/certmgrenv"
	certmgrpb "pixielabs.ai/pixielabs/src/vizier/services/certmgr/certmgrpb"
)

// K8sAPI is responsible for handing k8s requests.
type K8sAPI interface {
	CreateTLSSecret(name string, key string, cert string) error
	GetPodNamesForService(name string) ([]string, error)
	DeletePod(name string) error
}

// Server is an implementation of GRPC server for certmgr service.
type Server struct {
	env    certmgrenv.CertMgrEnv
	k8sAPI K8sAPI
}

// NewServer creates a new GRPC certmgr server.
func NewServer(env certmgrenv.CertMgrEnv, k8sAPI K8sAPI) *Server {
	return &Server{env: env, k8sAPI: k8sAPI}
}

// UpdateCerts updates the proxy certs with the given DNS address.
func (s *Server) UpdateCerts(ctx context.Context, req *certmgrpb.UpdateCertsRequest) (*certmgrpb.UpdateCertsResponse, error) {
	// Load secrets.
	err := s.k8sAPI.CreateTLSSecret("cloud-proxy-tls-certs", req.Key, req.Cert)
	if err != nil {
		return nil, err
	}

	// Bounce proxy service.
	pods, err := s.k8sAPI.GetPodNamesForService("vizier-proxy-service")
	if err != nil {
		return nil, err
	}

	if len(pods) != 1 {
		return nil, errors.New("No pods exist for proxy service")
	}

	err = s.k8sAPI.DeletePod(pods[0])

	if err != nil {
		return nil, err
	}

	return &certmgrpb.UpdateCertsResponse{
		OK: true,
	}, nil
}
