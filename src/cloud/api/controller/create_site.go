package controller

import (
	"encoding/json"
	"net/http"
	"strings"

	"pixielabs.ai/pixielabs/src/cloud/api/apienv"
	authpb "pixielabs.ai/pixielabs/src/cloud/auth/proto"
	"pixielabs.ai/pixielabs/src/cloud/site_manager/sitemanagerpb"
	"pixielabs.ai/pixielabs/src/shared/services"
	commonenv "pixielabs.ai/pixielabs/src/shared/services/env"
	"pixielabs.ai/pixielabs/src/shared/services/handler"
)

var emailDomainWhitelist = map[string]bool{
	"hulu.com":         true,
	"thousandeyes.com": true,
	"pixielabs.ai":     true,
	"shiftleft.io":     true,
	"umich.edu":        true,
}

// GetDomainNameFromEmail gets the domain name from the provided email.
func GetDomainNameFromEmail(email string) (string, error) {
	emailComponents := strings.Split(email, "@")
	if len(emailComponents) != 2 {
		return "", handler.NewStatusError(http.StatusBadRequest, "failed to parse request")
	}
	// If the user is not part of a whitelisted org, they should have an individual org.
	domainName := email
	if _, exists := emailDomainWhitelist[emailComponents[1]]; exists {
		domainName = emailComponents[1]
	}
	return domainName, nil
}

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
		DomainName  string
	}

	defer r.Body.Close()
	if err := json.NewDecoder(r.Body).Decode(&params); err != nil {
		return handler.NewStatusError(http.StatusBadRequest,
			"failed to decode json request")
	}

	domainName, err := GetDomainNameFromEmail(params.UserEmail)
	if err != nil {
		return err
	}

	rpcReq := &authpb.CreateUserOrgRequest{
		AccessToken: params.AccessToken,
		UserEmail:   params.UserEmail,
		DomainName:  domainName,
		OrgName:     domainName,
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
		DomainName: params.DomainName,
		OrgID:      resp.OrgID,
	}

	siteResp, err := apiEnv.SiteManagerClient().RegisterSite(ctxWithCreds, siteReq)
	if err != nil {
		return services.HTTPStatusFromError(err, "Failed to create site")
	}
	if !siteResp.SiteRegistered {
		return handler.NewStatusError(http.StatusInternalServerError, "Failed to create site")
	}

	setSessionCookie(session, resp.Token, resp.ExpiresAt, r, w)

	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(http.StatusOK)

	return nil
}
