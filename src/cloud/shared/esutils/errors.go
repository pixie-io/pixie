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

package esutils

import (
	"regexp"

	"github.com/olivere/elastic/v7"
)

var mergeFailureReg *regexp.Regexp
var settingsNonDynReg *regexp.Regexp

func init() {
	mergeFailureReg = regexp.MustCompile(
		`mapper \[(.*)\] of different type, current_type \[(.*)\], merged_type \[(.*)\]`)
	settingsNonDynReg = regexp.MustCompile(
		`Can't update non dynamic settings \[\[(.*)\]\] for open indices \[\[(.*)\]\]`)
}

type elasticErrorWrapper struct {
	e *elastic.Error
}

func mustParseError(err error) *elasticErrorWrapper {
	e := err.(*elastic.Error)
	return &elasticErrorWrapper{e}
}

func (e *elasticErrorWrapper) isMappingMergeFailure() bool {
	details := e.e.Details
	if details.Type != "illegal_argument_exception" {
		return false
	}
	return mergeFailureReg.Match([]byte(details.Reason))
}

func (e *elasticErrorWrapper) isSettingsNonDynamicUpdateFailure() bool {
	details := e.e.Details
	if details.Type != "illegal_argument_exception" {
		return false
	}
	return settingsNonDynReg.Match([]byte(details.Reason))
}
