package controller

import (
	"context"

	"pixielabs.ai/pixielabs/src/cloud/dnsmgr/dnsmgrenv"
	dnsmgrpb "pixielabs.ai/pixielabs/src/cloud/dnsmgr/dnsmgrpb"
)

// ResourceRecordTTL is the TTL of the resource record in seconds.
const ResourceRecordTTL = 30

// Server is an implementation of GRPC server for dnsmgr service.
type Server struct {
	env        dnsmgrenv.DNSMgrEnv
	dnsService DNSService
}

// NewServer creates a new GRPC dnsmgr server.
func NewServer(env dnsmgrenv.DNSMgrEnv, dnsService DNSService) *Server {
	return &Server{env: env, dnsService: dnsService}
}

// GetDNSAddress assigns and returns a DNS address for the given cluster ID and IP.
func (s *Server) GetDNSAddress(ctx context.Context, req *dnsmgrpb.GetDNSAddressRequest) (*dnsmgrpb.GetDNSAddressResponse, error) {
	// TOOD(michelle): Get a real DNS address name once the DB is implemented.
	dnsAddr := "test"

	err := s.dnsService.CreateResourceRecord(dnsAddr, req.IPAddress, ResourceRecordTTL)
	if err != nil {
		return nil, err
	}

	return &dnsmgrpb.GetDNSAddressResponse{
		DNSAddress: dnsAddr,
	}, nil
}
