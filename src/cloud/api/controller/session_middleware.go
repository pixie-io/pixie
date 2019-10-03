package controller

import (
	"fmt"
	"net/http"
	"strings"

	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/metadata"
	"google.golang.org/grpc/status"
	"pixielabs.ai/pixielabs/src/cloud/api/apienv"
	authpb "pixielabs.ai/pixielabs/src/cloud/auth/proto"
	"pixielabs.ai/pixielabs/src/shared/services/authcontext"
	"pixielabs.ai/pixielabs/src/shared/services/httpmiddleware"
)

// GetTokenFromSession gets a token from the session store using cookies.
func GetTokenFromSession(env apienv.APIEnv, r *http.Request) (string, bool) {
	session, err := GetDefaultSession(env, r)
	if err != nil {
		return "", false
	}

	// Check that the session is valid for the current subdomain.
	sessionDomain, ok := session.Values["_auth_site"]
	if !ok {
		return "", ok
	}

	if !strings.HasPrefix(r.Host, sessionDomain.(string)+".") {
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

		// Steps:
		// 1. Try to get the token out of session.
		// 2. If not try to get the session out bearer
		// 3. Generate augmented auth.

		token, ok := GetTokenFromSession(env, r)
		if !ok {
			// Try to get it from bearer.
			token, ok = httpmiddleware.GetTokenFromBearer(r)
			if !ok {
				http.Error(w, "failed to get auth token: either a bearer auth or valid cookie session must exist", http.StatusUnauthorized)
				return
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
			httpRetStatus := http.StatusInternalServerError
			if grpcCode == codes.Unauthenticated {
				httpRetStatus = http.StatusUnauthorized
			}
			http.Error(w, "failed to fetch token", httpRetStatus)
			return
		}

		aCtx := authcontext.New()
		err = aCtx.UseJWTAuth(env.JWTSigningKey(), resp.Token)
		if err != nil {
			http.Error(w, "Failed to parse token", http.StatusInternalServerError)
			return
		}

		newCtx := authcontext.NewContext(r.Context(), aCtx)
		ctxWithAugmentedAuth := metadata.AppendToOutgoingContext(newCtx, "authorization",
			fmt.Sprintf("bearer %s", resp.Token))
		next.ServeHTTP(w, r.WithContext(ctxWithAugmentedAuth))
	}
	return http.HandlerFunc(f)
}
