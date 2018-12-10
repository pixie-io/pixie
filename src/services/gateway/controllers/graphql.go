package controllers

import (
	"fmt"
	"net/http"
	"net/http/httputil"
	"net/url"
	"strings"

	"github.com/spf13/pflag"
	"github.com/spf13/viper"
	"pixielabs.ai/pixielabs/src/services/common/env"
	"pixielabs.ai/pixielabs/src/services/common/handler"
	"pixielabs.ai/pixielabs/src/services/common/sessioncontext"
)

func init() {
	pflag.String("api_service", "api-service:50201",
		"URL to the api service")
}

// GraphQLHandler is the HTTP handler for graphql requests.
func GraphQLHandler(e env.Env, w http.ResponseWriter, r *http.Request) error {
	sess, err := sessioncontext.FromContext(r.Context())
	if err != nil {
		return err
	}

	var apiServiceURL string
	if viper.GetBool("disable_ssl") {
		apiServiceURL = fmt.Sprintf("http://%s", viper.GetString("api_service"))
	} else {
		apiServiceURL = fmt.Sprintf("https://%s", viper.GetString("api_service"))
	}

	origin, err := url.Parse(apiServiceURL)
	if err != nil {
		return handler.NewStatusError(http.StatusInternalServerError, "failed to parse api service URL")
	}
	reverseProxy := httputil.NewSingleHostReverseProxy(origin)
	reverseProxy.Director = func(req *http.Request) {
		req.Header.Add("X-Forwarded-Host", req.Host)
		req.Header.Add("X-Origin-Host", origin.Host)
		req.Header.Add("Authorization", fmt.Sprintf("Bearer %s",
			sess.AuthToken))
		req.URL.Scheme = origin.Scheme
		req.URL.Host = origin.Host

		proxyPath := "/graphql"
		if strings.HasSuffix(proxyPath, "/") && len(proxyPath) > 1 {
			proxyPath = proxyPath[:len(proxyPath)-1]
		}
		req.URL.Path = proxyPath
	}

	reverseProxy.ServeHTTP(w, r)
	return nil
}
