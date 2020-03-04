package controller

import (
	"encoding/json"
	"net/http"

	"gopkg.in/segmentio/analytics-go.v3"
	"pixielabs.ai/pixielabs/src/shared/services/events"

	"pixielabs.ai/pixielabs/src/cloud/api/apienv"
	authpb "pixielabs.ai/pixielabs/src/cloud/auth/proto"
	"pixielabs.ai/pixielabs/src/cloud/site_manager/sitemanagerpb"
	"pixielabs.ai/pixielabs/src/shared/services"
	commonenv "pixielabs.ai/pixielabs/src/shared/services/env"
	"pixielabs.ai/pixielabs/src/shared/services/handler"
	"pixielabs.ai/pixielabs/src/utils"
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

	userIDStr := utils.UUIDFromProtoOrNil(resp.UserID).String()
	orgIDStr := utils.UUIDFromProtoOrNil(resp.OrgID).String()

	// TODO(nserrino): PL-1546 Move these to the Login API when this function is removed.
	events.Client().Enqueue(&analytics.Group{
		UserId:  userIDStr,
		GroupId: orgIDStr,
		Traits: map[string]interface{}{
			"kind":        "organization",
			"name":        resp.OrgName,
			"domain_name": resp.DomainName,
		},
	})

	events.Client().Enqueue(&analytics.Track{
		UserId: utils.UUIDFromProtoOrNil(resp.UserID).String(),
		Event:  events.OrgCreated,
		Properties: analytics.NewProperties().
			Set("org_id", orgIDStr),
	})

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

	events.Client().Enqueue(&analytics.Track{
		UserId: userIDStr,
		Event:  events.SiteCreated,
		Properties: analytics.NewProperties().
			Set("site_name", params.SiteName).
			Set("org_id", orgIDStr),
	})

	setSessionCookie(session, resp.Token, resp.ExpiresAt, params.SiteName, r, w)

	err = sendUserInfo(w, resp.UserInfo, resp.Token, resp.ExpiresAt, true)
	if err != nil {
		w.WriteHeader(http.StatusInternalServerError)
		return err
	}

	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(http.StatusOK)

	return nil
}
