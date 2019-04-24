package common

import (
	"fmt"

	log "github.com/sirupsen/logrus"
	"github.com/spf13/pflag"
	"github.com/spf13/viper"
)

// SetupService configures basic flags and defaults required by all services.
func SetupService(serviceName string, servicePortBase uint) {
	pflag.Uint("grpc_port", servicePortBase, fmt.Sprintf("The port to run the %s GRPC server", serviceName))
	pflag.Uint("http_port", servicePortBase+1, fmt.Sprintf("The port to run the %s HTTP server", serviceName))
	pflag.Bool("disable_ssl", false, "Disable SSL on the server")
	pflag.Bool("disable_grpc_auth", false, "Disable auth on the GRPC server")
	pflag.String("tls_key", "../certs/server.key", "The TLS key to use.")
	pflag.String("tls_cert", "../certs/server.crt", "The TLS certificate to use.")
	pflag.String("jwt_signing_key", "", "The signing key used for JWTs")
	pflag.String("external_addr", "", "The external address")
}

// PostFlagSetupAndParse does post setup flag config and parses them.
func PostFlagSetupAndParse() {
	pflag.Parse()

	// Must call after all flags are setup.
	viper.AutomaticEnv()
	viper.SetEnvPrefix("PL")
	viper.BindPFlags(pflag.CommandLine)
}

// CheckServiceFlags checks to make sure flag values are valid.
func CheckServiceFlags() {
	if len(viper.GetString("jwt_signing_key")) == 0 {
		log.Panic("Flag --jwt_signing_key or ENV PL_JWT_SIGNING_KEY is required")
	}

	if len(viper.GetString("external_addr")) == 0 {
		log.Panic("Flag --external_addr or ENV PL_EXTERNAL_ADDR is required")
	}

	if !viper.GetBool("disable_ssl") && len(viper.GetString("tls_key")) == 0 {
		log.Panic("Flag --tls_key or ENV PL_TLS_KEY is required when ssl is enabled")
	}

	if !viper.GetBool("disable_ssl") && len(viper.GetString("tls_cert")) == 0 {
		log.Panic("Flag --tls_cert or ENV PL_TLS_CERT is required when ssl is enabled")
	}

	if viper.GetBool("disable_grpc_auth") {
		log.Warn("Security WARNING!!! : Auth disabled on GRPC.")
	}
}
