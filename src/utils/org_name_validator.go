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

package utils

import (
	"errors"
	"strings"
	"unicode/utf8"
)

// ValidateOrgName checks to see if the specified org name is valid or not
// and returns nil for valid orgs, else returns and error informing why the
// name is considered invalid.
func ValidateOrgName(name string) error {
	if strings.ContainsAny(name, "./\\@$") {
		return errors.New("Org name contains disallowed characters (ex. $./\\@)")
	}
	if !utf8.ValidString(name) {
		return errors.New("Org name is not valid UTF-8")
	}
	if len(name) > 50 {
		return errors.New("Org name too long")
	}
	if len(name) < 6 {
		return errors.New("Org name too short")
	}
	return nil
}
