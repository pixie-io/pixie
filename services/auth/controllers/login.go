package controllers

import (
	"context"
	"encoding/json"
	"errors"
	"net/http"
	"time"

	"github.com/dgrijalva/jwt-go"
	"github.com/satori/go.uuid"
	"github.com/spf13/viper"
	pb "pixielabs.ai/pixielabs/services/auth/proto"
	jwtpb "pixielabs.ai/pixielabs/services/common/proto"
	"pixielabs.ai/pixielabs/services/common/utils"
)

// LoginResponse is the returned response from HTTP handler on successful login.
type LoginResponse struct {
	Token     string
	ExpiresAt int64
}

const (
	// TokenValidDuration is duration that the token is valid from current time.
	TokenValidDuration = 5 * 24 * time.Hour
)

// Login uses auth0 to authenticate and login the user.
func (s *Server) Login(ctx context.Context, request *pb.LoginRequest) (*pb.LoginReply, error) {
	return nil, nil
}

// GetAugmentedToken produces augmented tokens for the user based on passed in credentials.
func (s *Server) GetAugmentedToken(
	ctx context.Context, request *pb.GetAugmentedAuthTokenRequest) (
	*pb.GetAugmentedAuthTokenResponse, error) {
	return nil, nil
}

// NewHandleLoginFunc creates a login handler and initializes auth backend.
func NewHandleLoginFunc() (http.HandlerFunc, error) {
	jwtSigningKey := viper.GetString("jwt_signing_key")

	cfg := NewAuth0Config()
	auth0Connector := NewAuth0Connector(cfg)
	if err := auth0Connector.Init(); err != nil {
		return nil, errors.New("failed to initialize Auth0")
	}

	return MakeHandleLoginFunc(auth0Connector, jwtSigningKey), nil
}

// MakeHandleLoginFunc creates an HTTP handler and injects the auth connector.
func MakeHandleLoginFunc(a Auth0Connector, jwtSigningKey string) http.HandlerFunc {

	return func(w http.ResponseWriter, r *http.Request) {
		accessToken := r.FormValue("access_token")
		if accessToken == "" {
			http.Error(w, "missing access token", http.StatusUnauthorized)
			return
		}

		userID, err := a.GetUserIDFromToken(accessToken)
		if err != nil {
			http.Error(w, "failed to get user ID", http.StatusUnauthorized)
			return
		}

		// Make request to get user info.
		userInfo, err := a.GetUserInfo(userID)
		if err != nil {
			http.Error(w, "failed to get user info", http.StatusInternalServerError)
			return
		}

		// If it's a new user, then "register" by assigning a new
		// UUID.
		if userInfo.AppMetadata == nil || userInfo.AppMetadata.PLUserID == "" {
			userUUID := uuid.NewV4()
			err = a.SetPLUserID(userID, userUUID.String())
			if err != nil {
				http.Error(w, "failed to set user ID", http.StatusInternalServerError)
				return
			}

			// Read updated user info.
			userInfo, err = a.GetUserInfo(userID)
			if err != nil {
				http.Error(w, "failed to read updated user info", http.StatusInternalServerError)
				return
			}
		}

		expiresAt := time.Now().Add(TokenValidDuration)
		claims := generateJWTClaimsForUser(userInfo, expiresAt)
		token, err := signJWTClaims(claims, jwtSigningKey)

		if err != nil {
			http.Error(w, "failed to generate token", http.StatusInternalServerError)
		}

		resp := LoginResponse{
			Token:     token,
			ExpiresAt: expiresAt.Unix(),
		}

		json, err := json.Marshal(resp)
		if err != nil {
			http.Error(w, "failed to generate JSON response", http.StatusInternalServerError)
		}
		w.Write(json)
	}
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
