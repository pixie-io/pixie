package main

import (
	"net/http"

	"github.com/gorilla/sessions"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/pflag"
	"github.com/spf13/viper"
	"pixielabs.ai/pixielabs/services/common"
	"pixielabs.ai/pixielabs/services/common/healthz"
	"pixielabs.ai/pixielabs/services/gateway/controllers"
)

func setupServiceFlags() {
	pflag.String("session_key", "", "Cookie session key")
}

func setupServiceEnv() *controllers.GatewayEnv {
	sessionKey := viper.GetString("session_key")
	if len(sessionKey) == 0 {
		log.Fatal("session key is required")
	}
	sessionStore := sessions.NewCookieStore([]byte(viper.GetString("session_key")))
	return &controllers.GatewayEnv{
		Env: common.Env{
			ExternalAddress: viper.GetString("external_address"),
			SigningKey:      viper.GetString("jwt_signing_key"),
		},
		CookieStore: sessionStore,
	}
}

func main() {
	log.WithField("service", "gateway-service").Info("Starting service")

	common.SetupService("gateway-service", 50000)
	setupServiceFlags()
	common.PostFlagSetupAndParse()
	common.CheckServiceFlags()
	common.SetupServiceLogging()

	mux := http.NewServeMux()
	mux.Handle("/api/auth/login", http.HandlerFunc(controllers.AuthLoginHandler))
	mux.Handle("/api/auth/logout", http.HandlerFunc(controllers.AuthLogoutHandler))

	healthz.RegisterDefaultChecks(mux)
	s := common.NewPLServer(setupServiceEnv(), mux)
	s.Start()
	s.StopOnInterrupt()
}
