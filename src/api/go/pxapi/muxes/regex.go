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

package muxes

import (
	"context"
	"regexp"

	"px.dev/pixie/src/api/go/pxapi"
	"px.dev/pixie/src/api/go/pxapi/types"
)

// TableRecordHandlerFunc is called by the mux whenever a new table is streamed.
type TableRecordHandlerFunc func(metadata types.TableMetadata) (pxapi.TableRecordHandler, error)

type patternHandler struct {
	re          *regexp.Regexp
	handlerFunc TableRecordHandlerFunc
}

// RegexTableMux provides a regex based router for table based on table names.
type RegexTableMux struct {
	patterns []*patternHandler
}

// NewRegexTableMux creates a new default RegexTableMux.
func NewRegexTableMux() *RegexTableMux {
	return &RegexTableMux{}
}

// RegisterHandlerForPattern registers a handler function that is called based on the passed in regex pattern. The patterns are handled in the order specified.
func (r *RegexTableMux) RegisterHandlerForPattern(pattern string, handlerFunc TableRecordHandlerFunc) error {
	re, err := regexp.Compile(pattern)
	if err != nil {
		return err
	}
	r.patterns = append(r.patterns, &patternHandler{
		re:          re,
		handlerFunc: handlerFunc,
	})
	return nil
}

// AcceptTable implements the Muxer interface and is called when a new table is available.
func (r *RegexTableMux) AcceptTable(ctx context.Context, metadata types.TableMetadata) (pxapi.TableRecordHandler, error) {
	for _, pattern := range r.patterns {
		if pattern.re.Match([]byte(metadata.Name)) {
			return pattern.handlerFunc(metadata)
		}
	}
	return nil, nil
}
