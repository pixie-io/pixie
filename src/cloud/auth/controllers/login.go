package controllers

import (
	"context"
	"fmt"
	"time"

	uuid "github.com/satori/go.uuid"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/metadata"
	"google.golang.org/grpc/status"

	uuidpb "pixielabs.ai/pixielabs/src/api/public/uuidpb"
	pb "pixielabs.ai/pixielabs/src/cloud/auth/proto"
	profilepb "pixielabs.ai/pixielabs/src/cloud/profile/profilepb"
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
	// SupportAccountDomain is the domain name of the Pixie support account which can access the org provided at login.
	SupportAccountDomain = "pixie.support"
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

func (s *Server) updateAuthProviderUser(authUserID string, orgID string, userID string) (*UserInfo, error) {
	// Write user and org info to Auth0.
	err := s.a.SetPLMetadata(authUserID, orgID, userID)
	if err != nil {
		return nil, status.Error(codes.Internal, "failed to set user ID")
	}

	// Read updated user info.
	userInfo, err := s.a.GetUserInfo(authUserID)
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

	domainName, err := GetDomainNameFromEmail(userInfo.Email)
	if err != nil {
		return nil, services.HTTPStatusFromError(err, "Failed to get domain from email")
	}

	// If account is a Pixie support account, we don't want to create a new user.
	if domainName == SupportAccountDomain {
		return s.loginSupportUser(ctx, in, userInfo)
	} else if in.OrgName != "" {
		return nil, status.Error(codes.InvalidArgument, "orgName param not permitted for non Pixie support accounts")
	}

	pc := s.env.ProfileClient()

	// If user does not exist in the AuthProvider, then create a new user if specified.
	newUser := userInfo.PLUserID == ""

	// A user can exist in auth0, but not the profile service. If that's the case, we want to create a new user.
	if !newUser {
		_, err := pc.GetUser(ctx, &uuidpb.UUID{Data: []byte(userInfo.PLUserID)})
		if err != nil {
			newUser = true
		}
	}

	// If we can't find the user and aren't in auto create mode.
	if newUser && !in.CreateUserIfNotExists {
		return nil, status.Error(codes.PermissionDenied, "user not found, please register.")
	}

	// Users can login without registering if their org already exists. If org doesn't exist, they must complete sign up flow.
	orgInfo, err := pc.GetOrgByDomain(ctx, &profilepb.GetOrgByDomainRequest{DomainName: domainName})
	if err != nil || orgInfo == nil {
		return nil, status.Error(codes.InvalidArgument, "organization not found, please register.")
	}

	if newUser {
		userInfo, err = s.createUser(ctx, userID, userInfo, orgInfo)
		if err != nil {
			return nil, err
		}
	}

	// Update user's profile photo.
	_, err = pc.UpdateUser(ctx, &profilepb.UpdateUserRequest{
		ID:             &uuidpb.UUID{Data: []byte(userInfo.PLUserID)},
		ProfilePicture: userInfo.Picture,
	})
	if err != nil {
		return nil, err
	}

	token, expiresAt, err := generateJWTTokenForUser(userInfo, s.env.JWTSigningKey())
	if err != nil {
		return nil, status.Error(codes.Internal, "failed to generate token")
	}

	userID = userInfo.PLUserID

	return &pb.LoginReply{
		Token:       token,
		ExpiresAt:   expiresAt.Unix(),
		UserCreated: newUser,
		UserInfo: &pb.AuthenticatedUserInfo{
			UserID:    pbutils.ProtoFromUUIDStrOrNil(userID),
			FirstName: userInfo.FirstName,
			LastName:  userInfo.LastName,
			Email:     userInfo.Email,
		},
		OrgInfo: &pb.LoginReply_OrgInfo{
			OrgName: orgInfo.OrgName,
			OrgID:   pbutils.UUIDFromProtoOrNil(orgInfo.ID).String(),
		},
	}, nil
}

func (s *Server) loginSupportUser(ctx context.Context, in *pb.LoginRequest, userInfo *UserInfo) (*pb.LoginReply, error) {
	if in.OrgName == "" {
		return nil, status.Error(codes.InvalidArgument, "orgName is required for Pixie Support accounts")
	}

	pc := s.env.ProfileClient()

	orgInfo, err := pc.GetOrgByDomain(ctx, &profilepb.GetOrgByDomainRequest{DomainName: in.OrgName})
	if err != nil || orgInfo == nil {
		return nil, status.Error(codes.InvalidArgument, "organization not found")
	}

	// Generate token for impersonated support account.
	userID := uuid.FromStringOrNil("") // No account actually exists, so this should be a nil UUID.
	orgID := pbutils.UUIDFromProtoOrNil(orgInfo.ID)
	expiresAt := time.Now().Add(RefreshTokenValidDuration)
	claims := utils.GenerateJWTForUser(userID.String(), orgID.String(), userInfo.Email, expiresAt)
	token, err := utils.SignJWTClaims(claims, s.env.JWTSigningKey())
	if err != nil {
		return nil, status.Error(codes.Internal, "failed to generate token")
	}

	return &pb.LoginReply{
		Token:       token,
		ExpiresAt:   expiresAt.Unix(),
		UserCreated: false,
		UserInfo: &pb.AuthenticatedUserInfo{
			UserID:    pbutils.ProtoFromUUID(userID),
			FirstName: userInfo.FirstName,
			LastName:  userInfo.LastName,
			Email:     userInfo.Email,
		},
		OrgInfo: &pb.LoginReply_OrgInfo{
			OrgName: orgInfo.OrgName,
			OrgID:   orgID.String(),
		},
	}, nil
}

// Signup uses auth0 to authenticate and sign up the user. It autocreates the org if the org doesn't exist.
func (s *Server) Signup(ctx context.Context, in *pb.SignupRequest) (*pb.SignupReply, error) {
	userID, userInfo, err := s.getUserInfoFromToken(in.AccessToken)
	if err != nil {
		return nil, err
	}

	md, _ := metadata.FromIncomingContext(ctx)
	ctx = metadata.NewOutgoingContext(ctx, md)

	pc := s.env.ProfileClient()

	_, err = pc.GetUserByEmail(ctx, &profilepb.GetUserByEmailRequest{Email: userInfo.Email})
	if err == nil {
		return nil, status.Error(codes.PermissionDenied, "user already exists, please login.")
	}

	domainName, err := GetDomainNameFromEmail(userInfo.Email)
	if err != nil {
		return nil, services.HTTPStatusFromError(err, "Failed to get domain from email")
	}

	orgInfo, _ := pc.GetOrgByDomain(ctx, &profilepb.GetOrgByDomainRequest{DomainName: domainName})
	var orgID *uuidpb.UUID
	newOrg := orgInfo == nil
	if newOrg {
		userInfo, orgID, err = s.createUserAndOrg(ctx, domainName, userID, userInfo)
		if err != nil {
			return nil, err
		}
	} else {
		userInfo, err = s.createUser(ctx, userID, userInfo, orgInfo)
		orgID = orgInfo.ID
		if err != nil {
			return nil, err
		}
	}

	token, expiresAt, err := generateJWTTokenForUser(userInfo, s.env.JWTSigningKey())
	if err != nil {
		return nil, status.Error(codes.Internal, "failed to generate token")
	}

	return &pb.SignupReply{
		Token:      token,
		ExpiresAt:  expiresAt.Unix(),
		OrgCreated: newOrg,
		UserInfo: &pb.AuthenticatedUserInfo{
			UserID:    pbutils.ProtoFromUUIDStrOrNil(userInfo.PLUserID),
			FirstName: userInfo.FirstName,
			LastName:  userInfo.LastName,
			Email:     userInfo.Email,
		},
		OrgID: orgID,
	}, nil
}

// Creates a user as well as an org if the orgInfo passed in is nil.
func (s *Server) createUserAndOrg(ctx context.Context, domainName string, userID string, userInfo *UserInfo) (*UserInfo, *uuidpb.UUID, error) {
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

	pc := s.env.ProfileClient()
	resp, err := pc.CreateOrgAndUser(ctx, rpcReq)
	if err != nil {
		return nil, nil, status.Error(codes.Internal, "failed to create user/org")
	}
	orgIDpb := resp.OrgID
	userIDpb := resp.UserID

	userInfo, err = s.updateAuthProviderUser(userID, pbutils.UUIDFromProtoOrNil(orgIDpb).String(), pbutils.UUIDFromProtoOrNil(userIDpb).String())
	return userInfo, orgIDpb, err
}

// Creates a user in the passed in orgname.
func (s *Server) createUser(ctx context.Context, userID string, userInfo *UserInfo, orgInfo *profilepb.OrgInfo) (*UserInfo, error) {
	md, _ := metadata.FromIncomingContext(ctx)
	ctx = metadata.NewOutgoingContext(ctx, md)

	if orgInfo == nil {
		return nil, fmt.Errorf("orgInfo should not be nil")
	}
	// Create a new user to register them.
	userCreateReq := &profilepb.CreateUserRequest{
		OrgID:     orgInfo.ID,
		Username:  userInfo.Email,
		FirstName: userInfo.FirstName,
		LastName:  userInfo.LastName,
		Email:     userInfo.Email,
	}

	userIDpb, err := s.env.ProfileClient().CreateUser(ctx, userCreateReq)
	if err != nil {
		return nil, err
	}
	userInfo, err = s.updateAuthProviderUser(userID, pbutils.UUIDFromProtoOrNil(orgInfo.ID).String(), pbutils.UUIDFromProtoOrNil(userIDpb).String())
	return userInfo, err
}

// GetAugmentedTokenForAPIKey produces an augmented token for the user given a API key.
func (s *Server) GetAugmentedTokenForAPIKey(ctx context.Context, in *pb.GetAugmentedTokenForAPIKeyRequest) (*pb.GetAugmentedTokenForAPIKeyResponse, error) {
	// Find the org/user associated with the token.
	orgID, userID, err := s.apiKeyMgr.FetchOrgUserIDUsingAPIKey(ctx, in.APIKey)
	if err != nil {
		return nil, status.Errorf(codes.Unauthenticated, "Invalid API key")
	}

	// Generate service token, so that we can make a call to the Profile service.
	svcJWT := utils.GenerateJWTForService("AuthService")
	svcClaims, err := utils.SignJWTClaims(svcJWT, s.env.JWTSigningKey())
	if err != nil {
		return nil, status.Errorf(codes.Internal, "Failed to generate auth token")
	}
	ctxWithSvcCreds := metadata.AppendToOutgoingContext(ctx, "authorization",
		fmt.Sprintf("bearer %s", svcClaims))

	// Fetch user's email.
	pc := s.env.ProfileClient()
	user, err := pc.GetUser(ctxWithSvcCreds, pbutils.ProtoFromUUID(userID))
	if err != nil {
		return nil, status.Errorf(codes.Internal, "Failed to generate auth token")
	}

	// Create JWT for user/org.
	claims := utils.GenerateJWTForUser(userID.String(), orgID.String(), user.Email, time.Now().Add(AugmentedTokenValidDuration))
	token, err := utils.SignJWTClaims(claims, s.env.JWTSigningKey())
	if err != nil {
		return nil, status.Errorf(codes.Internal, "Failed to generate auth token")
	}

	resp := &pb.GetAugmentedTokenForAPIKeyResponse{
		Token:     token,
		ExpiresAt: claims.ExpiresAt,
	}
	return resp, nil
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

	if !aCtx.ValidClaims() {
		return nil, status.Error(codes.Unauthenticated, "Invalid auth/user")
	}

	// We perform extra checks for user tokens.
	if utils.GetClaimsType(aCtx.Claims) == utils.UserClaimType {
		// Check to make sure that the org and user exist in the system.
		pc := s.env.ProfileClient()

		md, _ := metadata.FromIncomingContext(ctx)
		ctx = metadata.NewOutgoingContext(ctx, md)

		orgIDstr := aCtx.Claims.GetUserClaims().OrgID
		_, err := pc.GetOrg(ctx, pbutils.ProtoFromUUIDStrOrNil(orgIDstr))
		if err != nil {
			return nil, status.Error(codes.Unauthenticated, "Invalid auth/org")
		}

		domainName, err := GetDomainNameFromEmail(aCtx.Claims.GetUserClaims().Email)
		if err != nil {
			return nil, status.Error(codes.Unauthenticated, "Invalid email")
		}

		if domainName != SupportAccountDomain {
			userIDstr := aCtx.Claims.GetUserClaims().UserID
			userInfo, err := pc.GetUser(ctx, pbutils.ProtoFromUUIDStrOrNil(userIDstr))
			if err != nil || userInfo == nil {
				return nil, status.Error(codes.Unauthenticated, "Invalid auth/user")
			}

			if orgIDstr != pbutils.UUIDFromProtoOrNil(userInfo.OrgID).String() {
				return nil, status.Error(codes.Unauthenticated, "Mismatched org")
			}
		}
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

func generateJWTTokenForUser(userInfo *UserInfo, signingKey string) (string, time.Time, error) {
	expiresAt := time.Now().Add(RefreshTokenValidDuration)
	claims := utils.GenerateJWTForUser(userInfo.PLUserID, userInfo.PLOrgID, userInfo.Email, expiresAt)
	token, err := utils.SignJWTClaims(claims, signingKey)

	return token, expiresAt, err
}
