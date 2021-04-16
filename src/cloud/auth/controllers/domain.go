package controllers

import (
	"net/http"
	"strings"

	"px.dev/pixie/src/shared/services/handler"
)

// Email domains from this list will create individual orgs.
var emailDomainExcludeOrgGroupList = map[string]bool{
	"gmail.com": true,
}

// GetDomainNameFromEmail gets the domain name from the provided email.
func GetDomainNameFromEmail(email string) (string, error) {
	emailComponents := strings.Split(email, "@")
	if len(emailComponents) != 2 {
		return "", handler.NewStatusError(http.StatusBadRequest, "failed to parse request")
	}
	// If the user is part of a excluded org, they should have an individual org.
	domainName := email
	if _, exists := emailDomainExcludeOrgGroupList[emailComponents[1]]; !exists {
		domainName = emailComponents[1]
	}
	return domainName, nil
}
