package controller

import (
	"encoding/json"
	"net/http"

	"pixielabs.ai/pixielabs/src/cloud/api/apienv"
	authpb "pixielabs.ai/pixielabs/src/cloud/auth/proto"
	"pixielabs.ai/pixielabs/src/cloud/site_manager/sitemanagerpb"
	"pixielabs.ai/pixielabs/src/shared/services"
	commonenv "pixielabs.ai/pixielabs/src/shared/services/env"
	"pixielabs.ai/pixielabs/src/shared/services/handler"
)

// CreateSiteHandler creates a new user/org and registers the site.
func CreateSiteHandler(env commonenv.Env, w http.ResponseWriter, r *http.Request) error {
	if r.Method != http.MethodPost {
		return handler.NewStatusError(http.StatusMethodNotAllowed, "not a post request")
	}

	session, err := getSessionFromEnv(env, r)
	if err != nil {
		return &handler.StatusError{http.StatusInternalServerError, err}
	}

	ctxWithCreds, err := attachCredentialsToContext(env, r)
	if err != nil {
		return &handler.StatusError{http.StatusInternalServerError, err}
	}

	var params struct {
		AccessToken string
		UserEmail   string
		State       string
		SiteName    string
	}

	defer r.Body.Close()
	if err := json.NewDecoder(r.Body).Decode(&params); err != nil {
		return handler.NewStatusError(http.StatusBadRequest,
			"failed to decode json request")
	}

	rpcReq := &authpb.CreateUserOrgRequest{
		AccessToken: params.AccessToken,
		UserEmail:   params.UserEmail,
	}

	apiEnv, ok := env.(apienv.APIEnv)
	if !ok {
		return handler.NewStatusError(http.StatusBadRequest, "failed to get environment")
	}

	resp, err := apiEnv.AuthClient().CreateUserOrg(ctxWithCreds, rpcReq)
	if err != nil {
		return services.HTTPStatusFromError(err, "Failed to create user/org")
	}

	siteReq := &sitemanagerpb.RegisterSiteRequest{
		SiteName: params.SiteName,
		OrgID:    resp.OrgID,
	}

	siteResp, err := apiEnv.SiteManagerClient().RegisterSite(ctxWithCreds, siteReq)
	if err != nil {
		return services.HTTPStatusFromError(err, "Failed to create site")
	}
	if !siteResp.SiteRegistered {
		return handler.NewStatusError(http.StatusInternalServerError, "Failed to create site")
	}

	setSessionCookie(session, resp.Token, resp.ExpiresAt, params.SiteName, r, w)

	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(http.StatusOK)

	return nil
}
