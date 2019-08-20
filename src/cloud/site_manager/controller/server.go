package controllers

import (
	"context"

	uuid "github.com/satori/go.uuid"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"
	"pixielabs.ai/pixielabs/src/cloud/site_manager/datastore"
	"pixielabs.ai/pixielabs/src/cloud/site_manager/sitemanagerenv"
	"pixielabs.ai/pixielabs/src/cloud/site_manager/sitemanagerpb"
	uuidpb "pixielabs.ai/pixielabs/src/common/uuid/proto"
	"pixielabs.ai/pixielabs/src/utils"
)

// SiteDatastore is the required interface for the backing data model.
type SiteDatastore interface {
	CheckAvailability(string) (bool, error)
	RegisterSite(uuid.UUID, string) error
	GetSiteForOrg(uuid.UUID) (*datastore.SiteInfo, error)
	GetSiteByDomain(string) (*datastore.SiteInfo, error)
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

	parsedOrgID, err := utils.UUIDFromProto(req.OrgID)
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

// GetSiteForOrg gets the site information based on the passed in ID.
func (s *Server) GetSiteForOrg(ctx context.Context, req *uuidpb.UUID) (*sitemanagerpb.SiteInfo, error) {
	parsedOrgID, err := utils.UUIDFromProto(req)
	if err != nil {
		return nil, err
	}

	siteInfo, err := s.datastore.GetSiteForOrg(parsedOrgID)

	if err != nil {
		return nil, status.Error(codes.Internal, err.Error())
	}
	if siteInfo == nil {
		return nil, status.Error(codes.NotFound, "not found")
	}

	resp := &sitemanagerpb.SiteInfo{}
	resp.DomainName = siteInfo.DomainName
	resp.OrgID = utils.ProtoFromUUID(&siteInfo.OrgID)

	return resp, nil
}

// GetSiteByDomain gets the site information based on the passed in domain name.
func (s *Server) GetSiteByDomain(ctx context.Context, req *sitemanagerpb.GetSiteByDomainRequest) (*sitemanagerpb.SiteInfo, error) {
	if len(req.DomainName) <= 0 {
		return nil, status.Error(codes.InvalidArgument, "domain name is a required argument")
	}
	var err error
	siteInfo, err := s.datastore.GetSiteByDomain(req.DomainName)
	if err != nil {
		return nil, status.Error(codes.Internal, err.Error())
	}
	if siteInfo == nil {
		return nil, status.Error(codes.NotFound, "not found")
	}

	resp := &sitemanagerpb.SiteInfo{}
	resp.DomainName = siteInfo.DomainName
	resp.OrgID = utils.ProtoFromUUID(&siteInfo.OrgID)

	return resp, nil
}
