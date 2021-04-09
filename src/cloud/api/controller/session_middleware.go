package controller

import (
	"context"
	"errors"
	"fmt"
	"net/http"

	"github.com/spf13/viper"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/metadata"
	"google.golang.org/grpc/status"
	"gopkg.in/segmentio/analytics-go.v3"

	"pixielabs.ai/pixielabs/src/cloud/api/apienv"
	authpb "pixielabs.ai/pixielabs/src/cloud/auth/authpb"
	"pixielabs.ai/pixielabs/src/shared/services/authcontext"
	"pixielabs.ai/pixielabs/src/shared/services/events"
	"pixielabs.ai/pixielabs/src/shared/services/httpmiddleware"
	"pixielabs.ai/pixielabs/src/shared/services/utils"
)

var (
	// ErrGetAuthTokenFailed occurs when we are unable to get a token from the cookie or bearer.
	ErrGetAuthTokenFailed = errors.New("failed to get auth token: either a bearer auth or valid cookie session must exist")
	// ErrFetchAugmentedTokenFailedInternal occurs when making a request for the augmented auth results in an internal error.
	ErrFetchAugmentedTokenFailedInternal = errors.New("failed to fetch token - internal")
	// ErrFetchAugmentedTokenFailedUnauthenticated occurs when making a request for the augmented token results in an authentication error.
	ErrFetchAugmentedTokenFailedUnauthenticated = errors.New("failed to fetch token - unauthenticated")
	// ErrParseAuthToken occurs when we are unable to parse the augmented token with the signing key.
	ErrParseAuthToken = errors.New("Failed to parse token")
)

// GetTokenFromSession gets a token from the session store using cookies.
func GetTokenFromSession(env apienv.APIEnv, r *http.Request) (string, bool) {
	session, err := GetDefaultSession(env, r)
	if err != nil {
		return "", false
	}

	accessToken, ok := session.Values["_at"].(string)
	if !ok {
		return "", ok
	}

	return accessToken, true
}

// WithAugmentedAuthMiddleware augments auth by send minimal token to the auth server and
// using returned data to augment the session.
func WithAugmentedAuthMiddleware(env apienv.APIEnv, next http.Handler) http.Handler {
	f := func(w http.ResponseWriter, r *http.Request) {
		ctx, err := getAugmentedAuthHTTP(env, r)
		if err != nil {
			if err == ErrFetchAugmentedTokenFailedUnauthenticated || err == ErrGetAuthTokenFailed {
				http.Error(w, err.Error(), http.StatusUnauthorized)
			} else {
				http.Error(w, err.Error(), http.StatusInternalServerError)
			}
			return
		}
		next.ServeHTTP(w, r.WithContext(ctx))
	}
	return http.HandlerFunc(f)
}

func getAugmentedToken(env apienv.APIEnv, r *http.Request) (string, error) {
	// Steps:
	// 1. Check if header contains a pixie-api-key. If so, generate augmented auth from the API Key.
	// 2. Try to get the token out of session.
	// 3. If not try to get the session out bearer
	// 4. Generate augmented auth.
	apiHeader := r.Header.Get("pixie-api-key")
	if apiHeader != "" {
		// Try to get augmented token.
		svcJWT := utils.GenerateJWTForService("APIService", viper.GetString("domain_name"))
		svcClaims, err := utils.SignJWTClaims(svcJWT, env.JWTSigningKey())
		if err != nil {
			return "", ErrGetAuthTokenFailed
		}
		ctxWithCreds := metadata.AppendToOutgoingContext(r.Context(), "authorization",
			fmt.Sprintf("bearer %s", svcClaims))

		apiKeyResp, err := env.AuthClient().GetAugmentedTokenForAPIKey(ctxWithCreds, &authpb.GetAugmentedTokenForAPIKeyRequest{
			APIKey: apiHeader,
		})
		if err == nil {
			// Get user/org info from augmented token.
			aCtx := authcontext.New()
			if err := aCtx.UseJWTAuth(env.JWTSigningKey(), apiKeyResp.Token, viper.GetString("domain_name")); err != nil {
				return "", ErrGetAuthTokenFailed
			}

			url := ""
			if r.URL != nil {
				url = r.URL.String()
			}
			events.Client().Enqueue(&analytics.Track{
				UserId: aCtx.Claims.GetUserClaims().UserID,
				Event:  events.APIRequest,
				Properties: analytics.NewProperties().
					Set("url", url).
					Set("api-client", r.Header.Get("pixie-api-client")),
			})

			return apiKeyResp.Token, nil
		}
	}

	token, ok := GetTokenFromSession(env, r)
	if !ok {
		// Try to get it from bearer.
		token, ok = httpmiddleware.GetTokenFromBearer(r)
		if !ok {
			return "", ErrGetAuthTokenFailed
		}
	}

	// Make a request to the Auth service to get an augmented token.
	// We don't need to check the token validity since the Auth service will just reject bad tokens.
	req := &authpb.GetAugmentedAuthTokenRequest{
		Token: token,
	}

	ctxWithCreds := metadata.AppendToOutgoingContext(r.Context(), "authorization",
		fmt.Sprintf("bearer %s", token))

	resp, err := env.AuthClient().GetAugmentedToken(ctxWithCreds, req)
	if err != nil {
		grpcCode := status.Code(err)
		if grpcCode == codes.Unauthenticated {
			return "", ErrFetchAugmentedTokenFailedUnauthenticated
		}
		return "", ErrFetchAugmentedTokenFailedInternal
	}
	return resp.Token, nil
}

func getAugmentedAuthHTTP(env apienv.APIEnv, r *http.Request) (context.Context, error) {
	token, err := getAugmentedToken(env, r)
	if err != nil {
		return nil, err
	}
	aCtx := authcontext.New()
	err = aCtx.UseJWTAuth(env.JWTSigningKey(), token, viper.GetString("domain_name"))
	if err != nil {
		return nil, ErrParseAuthToken
	}

	newCtx := authcontext.NewContext(r.Context(), aCtx)
	ctxWithAugmentedAuth := metadata.AppendToOutgoingContext(newCtx, "authorization",
		fmt.Sprintf("bearer %s", token))
	return ctxWithAugmentedAuth, nil
}

// GetAugmentedTokenGRPC gets the augmented token for a grpc context.
func GetAugmentedTokenGRPC(ctx context.Context, env apienv.APIEnv) (string, error) {
	md, ok := metadata.FromIncomingContext(ctx)
	if !ok {
		return "", errors.New("Could not get metadata from incoming md")
	}
	r := &http.Request{Header: http.Header{}}
	for k, v := range md {
		for _, val := range v {
			r.Header.Add(k, val)
		}
	}
	return getAugmentedToken(env, r)
}
