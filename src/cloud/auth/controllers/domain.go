package controllers

import (
	"net/http"
	"strings"

	"pixielabs.ai/pixielabs/src/shared/services/handler"
)

// Email domains from this list will create individual orgs.
var emailDomainBlacklist = map[string]bool{
	"gmail.com": true,
}

// GetDomainNameFromEmail gets the domain name from the provided email.
func GetDomainNameFromEmail(email string) (string, error) {
	emailComponents := strings.Split(email, "@")
	if len(emailComponents) != 2 {
		return "", handler.NewStatusError(http.StatusBadRequest, "failed to parse request")
	}
	// If the user is part of a blacklisted org, they should have an individual org.
	domainName := email
	if _, exists := emailDomainBlacklist[emailComponents[1]]; !exists {
		domainName = emailComponents[1]
	}
	return domainName, nil
}
