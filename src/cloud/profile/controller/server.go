package controller

import (
	"context"
	"errors"
	"strings"

	"github.com/badoux/checkmail"
	uuid "github.com/satori/go.uuid"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"

	"pixielabs.ai/pixielabs/src/cloud/profile/datastore"
	"pixielabs.ai/pixielabs/src/cloud/profile/profileenv"
	profile "pixielabs.ai/pixielabs/src/cloud/profile/profilepb"
	uuidpb "pixielabs.ai/pixielabs/src/common/uuid/proto"
	"pixielabs.ai/pixielabs/src/utils"
)

var emailDomainBlacklist = map[string]bool{
	"blacklist.com": true,
}

// Datastore is the interface used to the backing store for profile information.
type Datastore interface {
	// CreateUser creates a new user.
	CreateUser(*datastore.UserInfo) (uuid.UUID, error)
	// GetUser gets a user by ID.
	GetUser(uuid.UUID) (*datastore.UserInfo, error)

	// CreateUserAndOrg creates a user and org for creating a new org with specified user as owner.
	CreateUserAndOrg(*datastore.OrgInfo, *datastore.UserInfo) (orgID uuid.UUID, userID uuid.UUID, err error)
	// GetOrg gets and org by ID.
	GetOrg(uuid.UUID) (*datastore.OrgInfo, error)
	// GetOrgByDomain gets an org by domain name.
	GetOrgByDomain(string) (*datastore.OrgInfo, error)
}

// Server is an implementation of GRPC server for profile service.
type Server struct {
	env profileenv.ProfileEnv
	d   Datastore
}

// NewServer creates a new GRPC profile server.
func NewServer(env profileenv.ProfileEnv, d Datastore) *Server {
	return &Server{env: env, d: d}
}

func userInfoToProto(u *datastore.UserInfo) *profile.UserInfo {
	return &profile.UserInfo{
		ID:        utils.ProtoFromUUID(&u.ID),
		OrgID:     utils.ProtoFromUUID(&u.OrgID),
		Username:  u.Username,
		FirstName: u.FirstName,
		LastName:  u.LastName,
		Email:     u.Email,
	}
}

func orgInfoToProto(o *datastore.OrgInfo) *profile.OrgInfo {
	return &profile.OrgInfo{
		ID:         utils.ProtoFromUUID(&o.ID),
		OrgName:    o.OrgName,
		DomainName: o.DomainName,
	}
}

func checkValidEmail(email string) error {
	if len(email) == 0 || checkmail.ValidateFormat(email) != nil {
		return errors.New("failed validation")
	}

	components := strings.Split(email, "@")
	if len(components) != 2 {
		return errors.New("malformed email")
	}
	_, domain := components[0], components[1]

	if _, exists := emailDomainBlacklist[domain]; exists {
		return errors.New("disallowed email domain")
	}
	return nil
}

// CreateUser is the GRPC method to create  new user.
func (s *Server) CreateUser(ctx context.Context, req *profile.CreateUserRequest) (*uuidpb.UUID, error) {
	userInfo := &datastore.UserInfo{
		OrgID:     utils.UUIDFromProtoOrNil(req.OrgID),
		Username:  req.Username,
		FirstName: req.FirstName,
		LastName:  req.LastName,
		Email:     req.Email,
	}
	if userInfo.OrgID == uuid.Nil {
		return nil, status.Error(codes.InvalidArgument, "invalid org id")
	}
	if len(userInfo.Username) == 0 {
		return nil, status.Error(codes.InvalidArgument, "invalid username")
	}
	if len(userInfo.FirstName) == 0 {
		return nil, status.Error(codes.InvalidArgument, "invalid firstname")
	}
	if len(userInfo.LastName) == 0 {
		return nil, status.Error(codes.InvalidArgument, "invalid lastname")
	}
	if err := checkValidEmail(userInfo.Email); err != nil {
		return nil, status.Error(codes.InvalidArgument, err.Error())
	}
	uid, err := s.d.CreateUser(userInfo)
	return utils.ProtoFromUUID(&uid), err
}

// GetUser is the GRPC method to get a user.
func (s *Server) GetUser(ctx context.Context, req *uuidpb.UUID) (*profile.UserInfo, error) {
	uid := utils.UUIDFromProtoOrNil(req)
	userInfo, err := s.d.GetUser(uid)
	if err != nil {
		return nil, err
	}
	if userInfo == nil {
		return nil, status.Error(codes.NotFound, "no such user")
	}
	return userInfoToProto(userInfo), nil
}

// CreateOrgAndUser is the GRPC method to create a new org and user.
func (s *Server) CreateOrgAndUser(ctx context.Context, req *profile.CreateOrgAndUserRequest) (*profile.CreateOrgAndUserResponse, error) {
	orgInfo := &datastore.OrgInfo{
		DomainName: req.Org.DomainName,
		OrgName:    req.Org.OrgName,
	}

	userInfo := &datastore.UserInfo{
		Username:  req.User.Username,
		FirstName: req.User.FirstName,
		LastName:  req.User.LastName,
		Email:     req.User.Email,
	}
	if len(orgInfo.DomainName) == 0 {
		return nil, status.Error(codes.InvalidArgument, "invalid domain name")
	}
	if len(orgInfo.OrgName) == 0 {
		return nil, status.Error(codes.InvalidArgument, "invalid org name")
	}
	if len(userInfo.Username) == 0 {
		return nil, status.Error(codes.InvalidArgument, "invalid username")
	}
	if len(userInfo.FirstName) == 0 {
		return nil, status.Error(codes.InvalidArgument, "invalid firstname")
	}
	if len(userInfo.LastName) == 0 {
		return nil, status.Error(codes.InvalidArgument, "invalid lastname")
	}
	if err := checkValidEmail(userInfo.Email); err != nil {
		return nil, status.Error(codes.InvalidArgument, err.Error())
	}

	orgID, userID, err := s.d.CreateUserAndOrg(orgInfo, userInfo)
	if err != nil {
		return nil, err
	}

	resp := &profile.CreateOrgAndUserResponse{
		UserID: utils.ProtoFromUUID(&userID),
		OrgID:  utils.ProtoFromUUID(&orgID),
	}

	return resp, nil
}

// GetOrg is the GRPC method to get an org by ID.
func (s *Server) GetOrg(ctx context.Context, req *uuidpb.UUID) (*profile.OrgInfo, error) {
	orgID := utils.UUIDFromProtoOrNil(req)
	orgInfo, err := s.d.GetOrg(orgID)
	if err != nil {
		return nil, err
	}
	if orgInfo == nil {
		return nil, status.Error(codes.NotFound, "no such org")
	}
	return orgInfoToProto(orgInfo), nil
}

// GetOrgByDomain gets an org by domain name.
func (s *Server) GetOrgByDomain(ctx context.Context, req *profile.GetOrgByDomainRequest) (*profile.OrgInfo, error) {
	orgInfo, err := s.d.GetOrgByDomain(req.DomainName)
	if err != nil {
		return nil, err
	}
	if orgInfo == nil {
		return nil, status.Error(codes.NotFound, "no such org")
	}
	return orgInfoToProto(orgInfo), nil
}
