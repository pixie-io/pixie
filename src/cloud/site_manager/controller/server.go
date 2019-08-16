package controllers

import (
	"context"

	uuid "github.com/satori/go.uuid"
	"pixielabs.ai/pixielabs/src/cloud/site_manager/sitemanagerenv"
	"pixielabs.ai/pixielabs/src/cloud/site_manager/sitemanagerpb"
	"pixielabs.ai/pixielabs/src/utils"
)

// SiteDatastore is the required interface for the backing data model.
type SiteDatastore interface {
	CheckAvailability(string) (bool, error)
	RegisterSite(uuid.UUID, string) error
}

// Server defines an gRPC server type.
type Server struct {
	env       sitemanagerenv.SiteManagerEnv
	datastore SiteDatastore
}

// NewServer creates GRPC handlers.
func NewServer(env sitemanagerenv.SiteManagerEnv, datastore SiteDatastore) (*Server, error) {
	return &Server{
		env:       env,
		datastore: datastore,
	}, nil
}

// IsSiteAvailable checks to see if a site domain is available.
func (s *Server) IsSiteAvailable(ctx context.Context, req *sitemanagerpb.IsSiteAvailableRequest) (*sitemanagerpb.IsSiteAvailableResponse, error) {
	resp := &sitemanagerpb.IsSiteAvailableResponse{}
	isAvailable, err := s.datastore.CheckAvailability(req.DomainName)
	if err != nil {
		return nil, err
	}

	resp.Available = isAvailable
	return resp, nil
}

// RegisterSite registers a new site..
func (s *Server) RegisterSite(ctx context.Context, req *sitemanagerpb.RegisterSiteRequest) (*sitemanagerpb.RegisterSiteResponse, error) {
	resp := &sitemanagerpb.RegisterSiteResponse{}

	parsedOrgID, err := utils.UUIDFromProto(req.OrgId)
	if err != nil {
		return nil, err
	}
	// TODO(zasgar/michelle): We need to maybe have different error types.
	err = s.datastore.RegisterSite(parsedOrgID, req.DomainName)
	if err != nil {
		resp.SiteRegistered = false
		return resp, err
	}

	resp.SiteRegistered = true
	return resp, nil
}
