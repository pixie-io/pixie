package controller

import (
	"fmt"
	"net/http"

	log "github.com/sirupsen/logrus"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/metadata"
	"google.golang.org/grpc/status"
	"pixielabs.ai/pixielabs/src/services/api/apienv"
	authpb "pixielabs.ai/pixielabs/src/services/auth/proto"
	"pixielabs.ai/pixielabs/src/services/common/sessioncontext"
)

// WithSessionAuthMiddleware verifies that valid auth is present on the session.
// This needs to be included after the environment middleware
func WithSessionAuthMiddleware(env apienv.APIEnv, next http.Handler) http.Handler {
	f := func(w http.ResponseWriter, r *http.Request) {
		sess, err := sessioncontext.FromContext(r.Context())
		if err != nil {
			log.WithError(err).Error("Failed to extract session from context")
			http.Error(w, "internal environment error", http.StatusInternalServerError)
			return
		}

		session, err := GetDefaultSession(env, r)
		if err != nil {
			http.Error(w, "internal session error", http.StatusInternalServerError)
			return
		}

		accessToken, ok := session.Values["_at"].(string)
		err = sess.UseJWTAuth(env.JWTSigningKey(), accessToken)
		if err != nil || !ok {
			http.Error(w, "Invalid session credentials", http.StatusUnauthorized)
			return
		}

		next.ServeHTTP(w, r)
	}

	return http.HandlerFunc(f)
}

// WithAugmentedAuthMiddleware augments auth by send minimal token to the auth server and
// using returned data to augment the sesison.
func WithAugmentedAuthMiddleware(env apienv.APIEnv, next http.Handler) http.Handler {
	f := func(w http.ResponseWriter, r *http.Request) {
		sess, err := sessioncontext.FromContext(r.Context())
		if err != nil {
			log.WithError(err).Error("Failed to extract session from context")
			http.Error(w, "internal environment error", http.StatusInternalServerError)
			return
		}

		if len(sess.AuthToken) == 0 {
			log.Errorf("missing auth, this is likely a bug or config error in the middleware.")
			http.Error(w, "missing auth", http.StatusUnauthorized)
			return
		}

		req := &authpb.GetAugmentedAuthTokenRequest{
			Token: sess.AuthToken,
		}

		ctxWithCreds := metadata.AppendToOutgoingContext(r.Context(), "authorization",
			fmt.Sprintf("bearer %s", sess.AuthToken))
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

		err = sess.UseJWTAuth(env.JWTSigningKey(), resp.Token)
		if err != nil {
			http.Error(w, "Failed to parse token", http.StatusInternalServerError)
			return
		}

		next.ServeHTTP(w, r)

	}
	return http.HandlerFunc(f)
}
