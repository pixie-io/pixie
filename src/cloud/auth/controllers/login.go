package controllers

import (
	"context"
	"time"

	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/metadata"
	"google.golang.org/grpc/status"

	pb "pixielabs.ai/pixielabs/src/cloud/auth/proto"
	profilepb "pixielabs.ai/pixielabs/src/cloud/profile/profilepb"
	"pixielabs.ai/pixielabs/src/cloud/site_manager/sitemanagerpb"
	uuidpb "pixielabs.ai/pixielabs/src/common/uuid/proto"
	"pixielabs.ai/pixielabs/src/shared/services"
	"pixielabs.ai/pixielabs/src/shared/services/authcontext"
	"pixielabs.ai/pixielabs/src/shared/services/utils"
	pbutils "pixielabs.ai/pixielabs/src/utils"
)

const (
	// RefreshTokenValidDuration is duration that the refresh token is valid from current time.
	RefreshTokenValidDuration = 90 * 24 * time.Hour
	// AugmentedTokenValidDuration is the duration that the augmented token is valid from the current time.
	AugmentedTokenValidDuration = 30 * time.Minute
)

func (s *Server) getUserInfoFromToken(accessToken string) (string, *UserInfo, error) {
	if accessToken == "" {
		return "", nil, status.Error(codes.Unauthenticated, "missing access token")
	}

	userID, err := s.a.GetUserIDFromToken(accessToken)
	if err != nil {
		return "", nil, status.Error(codes.Unauthenticated, "failed to get user ID")
	}

	// Make request to get user info.
	userInfo, err := s.a.GetUserInfo(userID)
	if err != nil {
		return "", nil, status.Error(codes.Internal, "failed to get user info")
	}

	return userID, userInfo, nil
}

// CreateUserOrg creates a new user and organization and authenticates the user.
// TODO(nserrino): PL-1546 remove this and its callers now that Login does org creation.
func (s *Server) CreateUserOrg(ctx context.Context, in *pb.CreateUserOrgRequest) (*pb.CreateUserOrgResponse, error) {
	userID, userInfo, err := s.getUserInfoFromToken(in.AccessToken)
	if err != nil {
		return nil, err
	}

	if userInfo.Email != in.UserEmail {
		return nil, status.Error(codes.InvalidArgument, "email addresses don't match")
	}

	domainName, err := GetDomainNameFromEmail(userInfo.Email)
	if err != nil {
		return nil, services.HTTPStatusFromError(err, "Failed to get domain from email")
	}

	md, _ := metadata.FromIncomingContext(ctx)
	ctx = metadata.NewOutgoingContext(ctx, md)
	orgName := domainName

	rpcReq := &profilepb.CreateOrgAndUserRequest{
		Org: &profilepb.CreateOrgAndUserRequest_Org{
			OrgName:    orgName,
			DomainName: domainName,
		},
		User: &profilepb.CreateOrgAndUserRequest_User{
			Username:  userInfo.Email,
			FirstName: userInfo.FirstName,
			LastName:  userInfo.LastName,
			Email:     userInfo.Email,
		},
	}

	resp, err := s.env.ProfileClient().CreateOrgAndUser(ctx, rpcReq)
	if err != nil {
		return nil, status.Error(codes.Internal, "failed to create user/org")
	}

	userInfo, err = s.updateAuth0User(userID, pbutils.UUIDFromProtoOrNil(resp.OrgID).String(),
		pbutils.UUIDFromProtoOrNil(resp.UserID).String())

	token, expiresAt, err := generateJWTTokenForUser(userInfo, s.env.JWTSigningKey(), s.a.GetClientID())
	if err != nil {
		return nil, status.Error(codes.Internal, "failed to generate token")
	}

	userID = userInfo.AppMetadata[s.a.GetClientID()].PLUserID
	return &pb.CreateUserOrgResponse{
		Token:      token,
		ExpiresAt:  expiresAt.Unix(),
		UserID:     resp.UserID,
		OrgID:      resp.OrgID,
		OrgName:    orgName,
		DomainName: domainName,
		UserInfo: &pb.UserInfo{
			UserID:    pbutils.ProtoFromUUIDStrOrNil(userID),
			FirstName: userInfo.FirstName,
			LastName:  userInfo.LastName,
			Email:     userInfo.Email,
		},
	}, nil
}

func (s *Server) updateAuth0User(auth0UserID string, orgID string, userID string) (*UserInfo, error) {
	// Write user and org info to Auth0.
	err := s.a.SetPLMetadata(auth0UserID, orgID, userID)
	if err != nil {
		return nil, status.Error(codes.Internal, "failed to set user ID")
	}

	// Read updated user info.
	userInfo, err := s.a.GetUserInfo(auth0UserID)
	if err != nil {
		return nil, status.Error(codes.Internal, "failed to read updated user info")
	}

	return userInfo, nil
}

// Login uses auth0 to authenticate and login the user.
func (s *Server) Login(ctx context.Context, in *pb.LoginRequest) (*pb.LoginReply, error) {
	userID, userInfo, err := s.getUserInfoFromToken(in.AccessToken)
	if err != nil {
		return nil, err
	}

	md, _ := metadata.FromIncomingContext(ctx)
	ctx = metadata.NewOutgoingContext(ctx, md)

	pc := s.env.ProfileClient()
	sc := s.env.SiteManagerClient()

	// If user does not exist in Auth0, then create a new user if specified.
	newUser := userInfo.AppMetadata == nil || userInfo.AppMetadata[s.a.GetClientID()] == nil || userInfo.AppMetadata[s.a.GetClientID()].PLUserID == ""

	// If user exists in Auth0, but not in the profile service, create a new user.
	if !newUser {
		_, err := pc.GetUser(ctx, &uuidpb.UUID{Data: []byte(userInfo.AppMetadata[s.a.GetClientID()].PLUserID)})
		if err != nil {
			newUser = true
		}
	}

	// If we can't find the user and aren't in auto create mode.
	if newUser && !in.CreateUserIfNotExists {
		return nil, status.Error(codes.PermissionDenied, "user not found, please register.")
	}

	domainName, err := GetDomainNameFromEmail(userInfo.Email)
	if err != nil {
		return nil, services.HTTPStatusFromError(err, "Failed to get domain from email")
	}

	orgInfo, err := pc.GetOrgByDomain(ctx, &profilepb.GetOrgByDomainRequest{DomainName: domainName})
	if err != nil || orgInfo == nil {
		return nil, status.Error(codes.InvalidArgument, "organization not found, please register.")
	}

	siteInfo, err := sc.GetSiteByName(ctx, &sitemanagerpb.GetSiteByNameRequest{SiteName: in.SiteName})
	if err != nil || siteInfo == nil {
		return nil, status.Error(codes.InvalidArgument, "site does not exist")
	}

	if newUser {
		userInfo, _, err = s.createUserAndOptionallyOrg(ctx, domainName, userID, userInfo, orgInfo)
		if err != nil {
			return nil, err
		}
	}

	token, expiresAt, err := generateJWTTokenForUser(userInfo, s.env.JWTSigningKey(), s.a.GetClientID())
	if err != nil {
		return nil, status.Error(codes.Internal, "failed to generate token")
	}

	userID = userInfo.AppMetadata[s.a.GetClientID()].PLUserID

	return &pb.LoginReply{
		Token:       token,
		ExpiresAt:   expiresAt.Unix(),
		UserCreated: newUser,
		UserInfo: &pb.UserInfo{
			UserID:    pbutils.ProtoFromUUIDStrOrNil(userID),
			FirstName: userInfo.FirstName,
			LastName:  userInfo.LastName,
			Email:     userInfo.Email,
		},
	}, nil
}

// Signup uses auth0 to authenticate and sign up the user. It autocreates the org if the org doesn't exist.
func (s *Server) Signup(ctx context.Context, in *pb.SignupRequest) (*pb.SignupReply, error) {
	userID, userInfo, err := s.getUserInfoFromToken(in.AccessToken)
	if err != nil {
		return nil, err
	}

	if userInfo.Email != in.UserEmail {
		return nil, status.Error(codes.InvalidArgument, "email addresses don't match")
	}

	md, _ := metadata.FromIncomingContext(ctx)
	ctx = metadata.NewOutgoingContext(ctx, md)

	pc := s.env.ProfileClient()

	_, err = pc.GetUser(ctx, &uuidpb.UUID{Data: []byte(userID)})
	if err == nil {
		return nil, status.Error(codes.PermissionDenied, "user already exists, please login.")
	}

	domainName, err := GetDomainNameFromEmail(userInfo.Email)
	if err != nil {
		return nil, services.HTTPStatusFromError(err, "Failed to get domain from email")
	}

	orgInfo, _ := pc.GetOrgByDomain(ctx, &profilepb.GetOrgByDomainRequest{DomainName: domainName})
	newOrg := orgInfo == nil
	userInfo, orgID, err := s.createUserAndOptionallyOrg(ctx, domainName, userID, userInfo, orgInfo)
	if err != nil {
		return nil, err
	}

	token, expiresAt, err := generateJWTTokenForUser(userInfo, s.env.JWTSigningKey(), s.a.GetClientID())
	if err != nil {
		return nil, status.Error(codes.Internal, "failed to generate token")
	}

	userID = userInfo.AppMetadata[s.a.GetClientID()].PLUserID

	return &pb.SignupReply{
		Token:      token,
		ExpiresAt:  expiresAt.Unix(),
		OrgCreated: newOrg,
		UserInfo: &pb.UserInfo{
			UserID:    pbutils.ProtoFromUUIDStrOrNil(userID),
			FirstName: userInfo.FirstName,
			LastName:  userInfo.LastName,
			Email:     userInfo.Email,
		},
		OrgID: orgID,
	}, nil
}

// Creates a user as well as an org if the orgInfo passed in is nil.
func (s *Server) createUserAndOptionallyOrg(ctx context.Context, domainName string, userID string, userInfo *UserInfo, orgInfo *profilepb.OrgInfo) (*UserInfo, *uuidpb.UUID, error) {
	md, _ := metadata.FromIncomingContext(ctx)
	ctx = metadata.NewOutgoingContext(ctx, md)

	pc := s.env.ProfileClient()

	if orgInfo != nil {
		// Create a new user to register them.
		userCreateReq := &profilepb.CreateUserRequest{
			OrgID:     orgInfo.ID,
			Username:  userInfo.Email,
			FirstName: userInfo.FirstName,
			LastName:  userInfo.LastName,
			Email:     userInfo.Email,
		}

		resp, err := pc.CreateUser(ctx, userCreateReq)
		if err != nil {
			return nil, nil, err
		}

		userInfo, err := s.updateAuth0User(userID, pbutils.UUIDFromProtoOrNil(orgInfo.ID).String(),
			pbutils.UUIDFromProtoOrNil(resp).String())
		return userInfo, orgInfo.ID, err
	}

	orgName := domainName
	rpcReq := &profilepb.CreateOrgAndUserRequest{
		Org: &profilepb.CreateOrgAndUserRequest_Org{
			OrgName:    orgName,
			DomainName: domainName,
		},
		User: &profilepb.CreateOrgAndUserRequest_User{
			Username:  userInfo.Email,
			FirstName: userInfo.FirstName,
			LastName:  userInfo.LastName,
			Email:     userInfo.Email,
		},
	}

	resp, err := s.env.ProfileClient().CreateOrgAndUser(ctx, rpcReq)
	if err != nil {
		return nil, nil, status.Error(codes.Internal, "failed to create user/org")
	}

	userInfo, err = s.updateAuth0User(userID, pbutils.UUIDFromProtoOrNil(resp.OrgID).String(),
		pbutils.UUIDFromProtoOrNil(resp.UserID).String())

	return userInfo, resp.OrgID, err
}

// GetAugmentedToken produces augmented tokens for the user based on passed in credentials.
func (s *Server) GetAugmentedToken(
	ctx context.Context, in *pb.GetAugmentedAuthTokenRequest) (
	*pb.GetAugmentedAuthTokenResponse, error) {

	// Check the incoming token and make sure it's valid.
	aCtx := authcontext.New()

	if err := aCtx.UseJWTAuth(s.env.JWTSigningKey(), in.Token); err != nil {
		return nil, status.Errorf(codes.Unauthenticated, "Invalid auth token")
	}

	if !aCtx.ValidUser() {
		return nil, status.Error(codes.Unauthenticated, "Invalid auth/user")
	}

	// Check to make sure that the org and user exist in the system.
	pc := s.env.ProfileClient()

	md, _ := metadata.FromIncomingContext(ctx)
	ctx = metadata.NewOutgoingContext(ctx, md)

	orgIDstr := aCtx.Claims.GetUserClaims().OrgID
	_, err := pc.GetOrg(ctx, pbutils.ProtoFromUUIDStrOrNil(orgIDstr))
	if err != nil {
		return nil, status.Error(codes.Unauthenticated, "Invalid auth/org")
	}

	userIDstr := aCtx.Claims.GetUserClaims().UserID
	userInfo, err := pc.GetUser(ctx, pbutils.ProtoFromUUIDStrOrNil(userIDstr))
	if err != nil || userInfo == nil {
		return nil, status.Error(codes.Unauthenticated, "Invalid auth/user")
	}

	if orgIDstr != pbutils.UUIDFromProtoOrNil(userInfo.OrgID).String() {
		return nil, status.Error(codes.Unauthenticated, "Mismatched org")
	}

	// TODO(zasgar): This step should be to generate a new token base on what we get from a database.
	claims := *aCtx.Claims
	claims.IssuedAt = time.Now().Unix()
	claims.ExpiresAt = time.Now().Add(AugmentedTokenValidDuration).Unix()

	augmentedToken, err := utils.SignJWTClaims(&claims, s.env.JWTSigningKey())
	if err != nil {
		return nil, status.Error(codes.Internal, "failed to generate auth token")
	}
	// TODO(zasgar): This should actually do ACL's, etc. at some point.
	// Generate a new augmented auth token with additional information.
	resp := &pb.GetAugmentedAuthTokenResponse{
		Token:     augmentedToken,
		ExpiresAt: claims.ExpiresAt,
	}
	return resp, nil
}

func generateJWTTokenForUser(userInfo *UserInfo, signingKey string, clientID string) (string, time.Time, error) {
	expiresAt := time.Now().Add(RefreshTokenValidDuration)
	clientMetadata := userInfo.AppMetadata[clientID]
	claims := utils.GenerateJWTForUser(clientMetadata.PLUserID, clientMetadata.PLOrgID, userInfo.Email, expiresAt)
	token, err := utils.SignJWTClaims(claims, signingKey)

	return token, expiresAt, err
}
