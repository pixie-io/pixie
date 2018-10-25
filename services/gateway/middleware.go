package main

import (
	"context"
	"net/http"

	"github.com/gorilla/sessions"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/pflag"
	"github.com/spf13/viper"
	"pixielabs.ai/pixielabs/services/common"
	"pixielabs.ai/pixielabs/services/gateway/controllers"
)

func init() {
	pflag.String("session_key", "", "Cookie session key")
}

// WithEnvMiddleware creates a environment for the gateway service.
func WithEnvMiddleware(next http.Handler) http.Handler {
	sessionKey := viper.GetString("session_key")
	if len(sessionKey) == 0 {
		log.Fatal("session key is required")
	}
	sessionStore := sessions.NewCookieStore([]byte(viper.GetString("session_key")))
	env := &controllers.GatewayEnv{
		Env: common.Env{
			ExternalAddress: viper.GetString("external_address"),
			SigningKey:      viper.GetString("jwt_signing_key"),
		},
		CookieStore: sessionStore,
	}
	f := func(w http.ResponseWriter, r *http.Request) {
		ctx := context.WithValue(r.Context(), common.EnvKey, env)
		next.ServeHTTP(w, r.WithContext(ctx))
	}

	return http.HandlerFunc(f)
}
