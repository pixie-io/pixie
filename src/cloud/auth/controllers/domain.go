/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

package controllers

import (
	"net/http"
	"strings"

	"px.dev/pixie/src/shared/services/handler"
)

// Email domains from this list will create individual orgs.
var emailDomainExcludeOrgGroupList = map[string]bool{
	"gmail.com":      true,
	"googlemail.com": true,
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
