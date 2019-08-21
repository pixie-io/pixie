package controllers

import (
	"context"
	"strings"
	"time"

	"github.com/dgrijalva/jwt-go"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/metadata"
	"google.golang.org/grpc/status"

	pb "pixielabs.ai/pixielabs/src/cloud/auth/proto"
	profilepb "pixielabs.ai/pixielabs/src/cloud/profile/profilepb"
	"pixielabs.ai/pixielabs/src/shared/services/authcontext"
	jwtpb "pixielabs.ai/pixielabs/src/shared/services/proto"
	"pixielabs.ai/pixielabs/src/shared/services/utils"
	pbutils "pixielabs.ai/pixielabs/src/utils"
)

const (
	// RefreshTokenValidDuration is duration that the refresh token is valid from current time.
	RefreshTokenValidDuration = 5 * 24 * time.Hour
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
func (s *Server) CreateUserOrg(ctx context.Context, in *pb.CreateUserOrgRequest) (*pb.CreateUserOrgResponse, error) {
	userID, userInfo, err := s.getUserInfoFromToken(in.AccessToken)
	if err != nil {
		return nil, err
	}

	if userInfo.Email != in.UserEmail {
		return nil, status.Error(codes.InvalidArgument, "email addresses don't match")
	}

	if userInfo.AppMetadata != nil {
		return nil, status.Error(codes.InvalidArgument, "user already registered")
	}

	md, _ := metadata.FromIncomingContext(ctx)
	ctx = metadata.NewOutgoingContext(ctx, md)

	rpcReq := &profilepb.CreateOrgAndUserRequest{
		Org: &profilepb.CreateOrgAndUserRequest_Org{
			OrgName:    in.OrgName,
			DomainName: in.DomainName,
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

	token, expiresAt, err := generateJWTTokenForUser(userInfo, s.env.JWTSigningKey())
	if err != nil {
		return nil, status.Error(codes.Internal, "failed to generate token")
	}

	return &pb.CreateUserOrgResponse{
		Token:     token,
		ExpiresAt: expiresAt.Unix(),
		UserID:    resp.UserID,
		OrgID:     resp.OrgID,
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

	// If user does not exist, then create a new user.
	if userInfo.AppMetadata == nil || userInfo.AppMetadata.PLUserID == "" {
		userInfo, err = s.createUser(ctx, userID, userInfo)
		if err != nil {
			return nil, status.Error(codes.Internal, "failed to create new user")
		}
	}

	token, expiresAt, err := generateJWTTokenForUser(userInfo, s.env.JWTSigningKey())
	if err != nil {
		return nil, status.Error(codes.Internal, "failed to generate token")
	}

	return &pb.LoginReply{
		Token:     token,
		ExpiresAt: expiresAt.Unix(),
	}, nil
}

func (s *Server) createUser(ctx context.Context, userID string, userInfo *UserInfo) (*UserInfo, error) {
	emailComponents := strings.Split(userInfo.Email, "@")
	if len(emailComponents) != 2 {
		return nil, status.Error(codes.InvalidArgument, "invalid email address")
	}
	domainName := emailComponents[1]

	md, _ := metadata.FromIncomingContext(ctx)
	ctx = metadata.NewOutgoingContext(ctx, md)

	pc := s.env.ProfileClient()
	orgInfo, err := pc.GetOrgByDomain(ctx, &profilepb.GetOrgByDomainRequest{DomainName: domainName})
	if err != nil {
		return nil, err
	}

	if orgInfo == nil {
		return nil, status.Error(codes.InvalidArgument, "organization does not exist")
	}

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
		return nil, err
	}

	userInfo, err = s.updateAuth0User(userID, pbutils.UUIDFromProtoOrNil(orgInfo.ID).String(),
		pbutils.UUIDFromProtoOrNil(resp).String())

	return userInfo, nil
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

	// TODO(zasgar): This step should be to generate a new token base on what we get from a database.
	claims := *aCtx.Claims
	claims.IssuedAt = time.Now().Unix()
	claims.ExpiresAt = time.Now().Add(AugmentedTokenValidDuration).Unix()

	augmentedToken, err := signJWTClaims(&claims, s.env.JWTSigningKey())
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

func generateJWTClaimsForUser(userInfo *UserInfo, expiresAt time.Time) *jwtpb.JWTClaims {
	claims := jwtpb.JWTClaims{
		Subject: userInfo.AppMetadata.PLUserID,
		UserID:  userInfo.AppMetadata.PLUserID,
		Email:   userInfo.Email,
		// Standard claims.
		ExpiresAt: expiresAt.Unix(),
		IssuedAt:  time.Now().Unix(),
		Issuer:    "PL",
	}
	return &claims
}

func signJWTClaims(claims *jwtpb.JWTClaims, signingKey string) (string, error) {
	mc := utils.PBToMapClaims(claims)
	return jwt.NewWithClaims(jwt.SigningMethodHS256, mc).SignedString([]byte(signingKey))
}

func generateJWTTokenForUser(userInfo *UserInfo, signingKey string) (string, time.Time, error) {
	expiresAt := time.Now().Add(RefreshTokenValidDuration)
	claims := generateJWTClaimsForUser(userInfo, expiresAt)
	token, err := signJWTClaims(claims, signingKey)

	return token, expiresAt, err
}
