package controllers

import (
	"context"
	"fmt"
	"regexp"
	"strings"

	uuid "github.com/satori/go.uuid"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"
	"pixielabs.ai/pixielabs/src/cloud/site_manager/datastore"
	"pixielabs.ai/pixielabs/src/cloud/site_manager/sitemanagerpb"
	uuidpb "pixielabs.ai/pixielabs/src/common/uuid/proto"
	"pixielabs.ai/pixielabs/src/utils"
)

var subdomainRegex = regexp.MustCompile("^([a-z0-9])+(-[a-z0-9]+)*$")

// SiteDatastore is the required interface for the backing data model.
type SiteDatastore interface {
	CheckAvailability(string) (bool, error)
	RegisterSite(uuid.UUID, string) error
	GetSiteForOrg(uuid.UUID) (*datastore.SiteInfo, error)
	GetSiteByName(string) (*datastore.SiteInfo, error)
}

// Server defines an gRPC server type.
type Server struct {
	datastore SiteDatastore
}

// NewServer creates GRPC handlers.
func NewServer(datastore SiteDatastore) *Server {
	return &Server{
		datastore: datastore,
	}
}

var siteNameBlacklist = map[string]bool{
	"cloud": true,
	"id":    true,
}

func validSubdomain(s string) bool {
	return subdomainRegex.MatchString(s)
}

// IsSiteAvailable checks to see if a site is available.
func (s *Server) IsSiteAvailable(ctx context.Context, req *sitemanagerpb.IsSiteAvailableRequest) (*sitemanagerpb.IsSiteAvailableResponse, error) {
	resp := &sitemanagerpb.IsSiteAvailableResponse{}
	sn := strings.ToLower(req.SiteName)
	if _, exists := siteNameBlacklist[sn]; exists {
		resp.Available = false
		return resp, nil
	}

	if !validSubdomain(sn) {
		return nil, status.Error(codes.InvalidArgument, fmt.Sprintf("site name must consist of only a-z and 0-9"))
	}

	isAvailable, err := s.datastore.CheckAvailability(sn)
	if err != nil {
		return nil, err
	}

	resp.Available = isAvailable
	return resp, nil
}

// RegisterSite registers a new site..
func (s *Server) RegisterSite(ctx context.Context, req *sitemanagerpb.RegisterSiteRequest) (*sitemanagerpb.RegisterSiteResponse, error) {
	resp := &sitemanagerpb.RegisterSiteResponse{}
	sn := strings.ToLower(req.SiteName)

	if _, exists := siteNameBlacklist[sn]; exists {
		return nil, status.Error(codes.InvalidArgument, fmt.Sprintf("cannot register site with name: %s", sn))
	}

	if !validSubdomain(sn) {
		return nil, status.Error(codes.InvalidArgument, fmt.Sprintf("site name must consist of only a-z and 0-9"))
	}

	parsedOrgID, err := utils.UUIDFromProto(req.OrgID)
	if err != nil {
		return nil, err
	}
	// TODO(zasgar/michelle): We need to maybe have different error types.
	err = s.datastore.RegisterSite(parsedOrgID, sn)
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
	resp.SiteName = siteInfo.SiteName
	resp.OrgID = utils.ProtoFromUUID(&siteInfo.OrgID)

	return resp, nil
}

// GetSiteByName gets the site information based on the passed in site name.
func (s *Server) GetSiteByName(ctx context.Context, req *sitemanagerpb.GetSiteByNameRequest) (*sitemanagerpb.SiteInfo, error) {
	sn := strings.ToLower(req.SiteName)

	if len(sn) <= 0 {
		return nil, status.Error(codes.InvalidArgument, "site name is a required argument")
	}
	var err error
	siteInfo, err := s.datastore.GetSiteByName(sn)
	if err != nil {
		return nil, status.Error(codes.Internal, err.Error())
	}
	if siteInfo == nil {
		return nil, status.Error(codes.NotFound, "not found")
	}

	resp := &sitemanagerpb.SiteInfo{}
	resp.SiteName = siteInfo.SiteName
	resp.OrgID = utils.ProtoFromUUID(&siteInfo.OrgID)

	return resp, nil
}
