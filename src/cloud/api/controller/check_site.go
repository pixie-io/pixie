package controller

import (
	"fmt"
	"net/http"

	"github.com/gogo/protobuf/jsonpb"
	log "github.com/sirupsen/logrus"
	"google.golang.org/grpc/metadata"
	"pixielabs.ai/pixielabs/src/cloud/api/apienv"
	"pixielabs.ai/pixielabs/src/cloud/site_manager/sitemanagerpb"
)

// CheckSiteHandler defines the HTTP handlers for the site checker.
type CheckSiteHandler struct {
	env apienv.APIEnv
}

// HandlerFunc is an http.handlerfunc that will make requests to the site manager server.
func (c *CheckSiteHandler) HandlerFunc(w http.ResponseWriter, r *http.Request) {
	sc := c.env.SiteManagerClient()
	keys, ok := r.URL.Query()["site_name"]

	if !ok || len(keys[0]) < 1 {
		http.Error(w, http.StatusText(http.StatusBadRequest)+" : missing site name", http.StatusBadRequest)
		return
	}

	siteName := keys[0]

	if len(siteName) < 3 {
		http.Error(w, http.StatusText(http.StatusBadRequest)+" : site name should be atleast 3 characters", http.StatusBadRequest)
		return
	}

	reqPB := &sitemanagerpb.IsSiteAvailableRequest{
		SiteName: siteName,
	}

	serviceAuthToken, err := GetServiceCredentials(c.env.JWTSigningKey())
	if err != nil {
		log.WithError(err).Error("Service authpb failure")
		http.Error(w, http.StatusText(http.StatusInternalServerError), http.StatusInternalServerError)
	}

	ctxWithCreds := metadata.AppendToOutgoingContext(r.Context(), "authorization",
		fmt.Sprintf("bearer %s", serviceAuthToken))

	resp, err := sc.IsSiteAvailable(ctxWithCreds, reqPB)

	if err != nil {
		log.WithError(err).Error("grpc request failed")
		http.Error(w, http.StatusText(http.StatusInternalServerError), http.StatusInternalServerError)
		return
	}

	marshaler := &jsonpb.Marshaler{EmitDefaults: true}
	err = marshaler.Marshal(w, resp)

	if err != nil {
		log.WithError(err).Error("failed to marshal proto")
		http.Error(w, http.StatusText(http.StatusInternalServerError), http.StatusInternalServerError)
		return
	}
}

// NewCheckSiteHandler creates a CheckSiteHandler.
func NewCheckSiteHandler(env apienv.APIEnv) *CheckSiteHandler {
	h := &CheckSiteHandler{env}
	return h
}
