package controllers

import (
	"context"
	"time"

	"github.com/dgrijalva/jwt-go"
	uuid "github.com/satori/go.uuid"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"
	pb "pixielabs.ai/pixielabs/src/cloud/auth/proto"
	"pixielabs.ai/pixielabs/src/shared/services/authcontext"
	jwtpb "pixielabs.ai/pixielabs/src/shared/services/proto"
	"pixielabs.ai/pixielabs/src/shared/services/utils"
)

const (
	// RefreshTokenValidDuration is duration that the refresh token is valid from current time.
	RefreshTokenValidDuration = 5 * 24 * time.Hour
	// AugmentedTokenValidDuration is the duration that the augmented token is valid from the current time.
	AugmentedTokenValidDuration = 30 * time.Minute
)

// Login uses auth0 to authenticate and login the user.
func (s *Server) Login(ctx context.Context, in *pb.LoginRequest) (*pb.LoginReply, error) {
	accessToken := in.AccessToken
	if accessToken == "" {
		return nil, status.Error(codes.Unauthenticated, "missing access token")
	}

	userID, err := s.a.GetUserIDFromToken(accessToken)
	if err != nil {
		return nil, status.Error(codes.Unauthenticated, "failed to get user ID")
	}

	// Make request to get user info.
	userInfo, err := s.a.GetUserInfo(userID)
	if err != nil {
		return nil, status.Error(codes.Internal, "failed to get user info")
	}

	// If it's a new user, then "register" by assigning a new
	// UUID.
	if userInfo.AppMetadata == nil || userInfo.AppMetadata.PLUserID == "" {
		userUUID := uuid.NewV4()
		err = s.a.SetPLUserID(userID, userUUID.String())
		if err != nil {
			return nil, status.Error(codes.Internal, "failed to set user ID")
		}

		// Read updated user info.
		userInfo, err = s.a.GetUserInfo(userID)
		if err != nil {
			return nil, status.Error(codes.Internal, "failed to read updated user info")
		}
	}

	expiresAt := time.Now().Add(RefreshTokenValidDuration)
	claims := generateJWTClaimsForUser(userInfo, expiresAt)
	token, err := signJWTClaims(claims, s.env.JWTSigningKey())

	if err != nil {
		return nil, status.Error(codes.Internal, "failed to generate token")
	}

	return &pb.LoginReply{
		Token:     token,
		ExpiresAt: expiresAt.Unix(),
	}, nil
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
