package controllers

import (
	"context"
	"time"

	"github.com/dgrijalva/jwt-go"
	"github.com/satori/go.uuid"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"
	pb "pixielabs.ai/pixielabs/services/auth/proto"
	jwtpb "pixielabs.ai/pixielabs/services/common/proto"
	"pixielabs.ai/pixielabs/services/common/sessioncontext"
	"pixielabs.ai/pixielabs/services/common/utils"
)

const (
	// TokenValidDuration is duration that the token is valid from current time.
	TokenValidDuration = 5 * 24 * time.Hour
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

	expiresAt := time.Now().Add(TokenValidDuration)
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
	sessionCtx, err := sessioncontext.FromContext(ctx)
	if err != nil {
		return nil, err
	}
	// TODO(zasgar): This should actually do ACL's, etc. at some point.
	resp := &pb.GetAugmentedAuthTokenResponse{
		Token:  in.Token,
		Claims: sessionCtx.Claims,
	}
	return resp, nil
}

func generateJWTClaimsForUser(userInfo *UserInfo, expiresAt time.Time) *jwtpb.JWTClaims {
	claims := jwtpb.JWTClaims{
		UserID: userInfo.AppMetadata.PLUserID,
		Email:  userInfo.Email,
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
