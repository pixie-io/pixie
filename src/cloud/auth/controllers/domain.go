package controllers

import (
	"net/http"
	"strings"

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
