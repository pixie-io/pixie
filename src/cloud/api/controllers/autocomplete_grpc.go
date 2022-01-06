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
	"context"

	"github.com/gofrs/uuid"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"

	"px.dev/pixie/src/api/proto/cloudpb"
	"px.dev/pixie/src/cloud/autocomplete"
	"px.dev/pixie/src/shared/services/authcontext"
)

// AutocompleteServer is the server that implements the Autocomplete gRPC service.
type AutocompleteServer struct {
	Suggester autocomplete.Suggester
}

// Autocomplete returns a formatted string and autocomplete suggestions.
func (a *AutocompleteServer) Autocomplete(ctx context.Context, req *cloudpb.AutocompleteRequest) (*cloudpb.AutocompleteResponse, error) {
	sCtx, err := authcontext.FromContext(ctx)
	if err != nil {
		return nil, err
	}
	orgIDstr := sCtx.Claims.GetUserClaims().OrgID
	orgID, err := uuid.FromString(orgIDstr)
	if err != nil {
		return nil, err
	}

	fmtString, executable, suggestions, err := autocomplete.Autocomplete(req.Input, int(req.CursorPos), req.Action, a.Suggester, orgID, req.ClusterUID)
	if err != nil {
		return nil, err
	}

	return &cloudpb.AutocompleteResponse{
		FormattedInput: fmtString,
		IsExecutable:   executable,
		TabSuggestions: suggestions,
	}, nil
}

// AutocompleteField returns suggestions for a single field.
func (a *AutocompleteServer) AutocompleteField(ctx context.Context, req *cloudpb.AutocompleteFieldRequest) (*cloudpb.AutocompleteFieldResponse, error) {
	sCtx, err := authcontext.FromContext(ctx)
	if err != nil {
		return nil, err
	}
	orgIDstr := sCtx.Claims.GetUserClaims().OrgID
	orgID, err := uuid.FromString(orgIDstr)
	if err != nil {
		return nil, err
	}

	allowedArgs := []cloudpb.AutocompleteEntityKind{}
	if req.RequiredArgTypes != nil {
		allowedArgs = req.RequiredArgTypes
	}

	suggestionReq := []*autocomplete.SuggestionRequest{
		{
			OrgID:        orgID,
			Input:        req.Input,
			AllowedKinds: []cloudpb.AutocompleteEntityKind{req.FieldType},
			AllowedArgs:  allowedArgs,
			ClusterUID:   req.ClusterUID,
		},
	}
	suggestions, err := a.Suggester.GetSuggestions(suggestionReq)
	if err != nil {
		return nil, err
	}
	if len(suggestions) != 1 {
		return nil, status.Error(codes.Internal, "failed to get autocomplete suggestions")
	}

	acSugg := make([]*cloudpb.AutocompleteSuggestion, len(suggestions[0].Suggestions))
	for j, s := range suggestions[0].Suggestions {
		acSugg[j] = &cloudpb.AutocompleteSuggestion{
			Kind:           s.Kind,
			Name:           s.Name,
			Description:    s.Desc,
			MatchedIndexes: s.MatchedIndexes,
			State:          s.State,
		}
	}

	return &cloudpb.AutocompleteFieldResponse{
		Suggestions:          acSugg,
		HasAdditionalMatches: suggestions[0].HasAdditionalMatches,
	}, nil
}
