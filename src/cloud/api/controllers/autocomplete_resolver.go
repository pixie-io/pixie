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
	"errors"

	"px.dev/pixie/src/api/proto/cloudpb"
)

var actionToProtoMap = map[string]cloudpb.AutocompleteActionType{
	"AAT_UNKNOWN": cloudpb.AAT_UNKNOWN,
	"AAT_EDIT":    cloudpb.AAT_EDIT,
	"AAT_SELECT":  cloudpb.AAT_SELECT,
}

var protoToKindMap = map[cloudpb.AutocompleteEntityKind]string{
	cloudpb.AEK_UNKNOWN:   "AEK_UNKNOWN",
	cloudpb.AEK_POD:       "AEK_POD",
	cloudpb.AEK_SVC:       "AEK_SVC",
	cloudpb.AEK_SCRIPT:    "AEK_SCRIPT",
	cloudpb.AEK_NAMESPACE: "AEK_NAMESPACE",
	cloudpb.AEK_NODE:      "AEK_NODE",
}

var kindToProtoMap = map[string]cloudpb.AutocompleteEntityKind{
	"AEK_UNKNOWN":   cloudpb.AEK_UNKNOWN,
	"AEK_POD":       cloudpb.AEK_POD,
	"AEK_SVC":       cloudpb.AEK_SVC,
	"AEK_SCRIPT":    cloudpb.AEK_SCRIPT,
	"AEK_NAMESPACE": cloudpb.AEK_NAMESPACE,
	"AEK_NODE":      cloudpb.AEK_NODE,
}

var protoToStateMap = map[cloudpb.AutocompleteEntityState]string{
	cloudpb.AES_UNKNOWN:    "AES_UNKNOWN",
	cloudpb.AES_PENDING:    "AES_PENDING",
	cloudpb.AES_RUNNING:    "AES_RUNNING",
	cloudpb.AES_FAILED:     "AES_FAILED",
	cloudpb.AES_TERMINATED: "AES_TERMINATED",
}

// AutocompleteField is the resolver for autocompleting a single field.
func (q *QueryResolver) AutocompleteField(ctx context.Context, args *autocompleteFieldArgs) (*AutocompleteFieldResolver, error) {
	grpcAPI := q.Env.AutocompleteServer
	allowedArgs := make([]cloudpb.AutocompleteEntityKind, 0)
	if args.RequiredArgTypes != nil {
		for _, a := range *args.RequiredArgTypes {
			allowedArgs = append(allowedArgs, kindToProtoMap[*a])
		}
	}
	if args.FieldType == nil {
		return nil, errors.New("field type required")
	}

	clusterUID := ""
	if args.ClusterUID != nil {
		clusterUID = *(args.ClusterUID)
	}

	res, err := grpcAPI.AutocompleteField(ctx, &cloudpb.AutocompleteFieldRequest{
		Input:            *args.Input,
		FieldType:        kindToProtoMap[*args.FieldType],
		RequiredArgTypes: allowedArgs,
		ClusterUID:       clusterUID,
	})
	if err != nil {
		return nil, rpcErrorHelper(err)
	}

	suggestions := make([]*AutocompleteSuggestion, len(res.Suggestions))
	for j := range res.Suggestions {
		kind := protoToKindMap[res.Suggestions[j].Kind]
		state := protoToStateMap[res.Suggestions[j].State]
		idxs := make([]*int32, len(res.Suggestions[j].MatchedIndexes))
		for k, idx := range res.Suggestions[j].MatchedIndexes {
			castedIdx := int32(idx)
			idxs[k] = &castedIdx
		}
		suggestions[j] = &AutocompleteSuggestion{
			Kind:           &kind,
			Name:           &res.Suggestions[j].Name,
			Description:    &res.Suggestions[j].Description,
			MatchedIndexes: &idxs,
			State:          &state,
		}
	}

	return &AutocompleteFieldResolver{
		Suggestions:          suggestions,
		HasAdditionalMatches: res.HasAdditionalMatches,
	}, nil
}

// Autocomplete responds to an autocomplete request.
func (q *QueryResolver) Autocomplete(ctx context.Context, args *autocompleteArgs) (*AutocompleteResolver, error) {
	clusterUID := ""
	if args.ClusterUID != nil {
		clusterUID = *(args.ClusterUID)
	}

	grpcAPI := q.Env.AutocompleteServer
	res, err := grpcAPI.Autocomplete(ctx, &cloudpb.AutocompleteRequest{
		Input:      *args.Input,
		CursorPos:  int64(*args.CursorPos),
		Action:     actionToProtoMap[*args.Action],
		ClusterUID: clusterUID,
	})
	if err != nil {
		return nil, rpcErrorHelper(err)
	}

	suggestions := make([]*TabSuggestion, len(res.TabSuggestions))
	for i, s := range res.TabSuggestions {
		as := make([]*AutocompleteSuggestion, len(s.Suggestions))
		for j := range s.Suggestions {
			kind := protoToKindMap[s.Suggestions[j].Kind]
			state := protoToStateMap[s.Suggestions[j].State]
			idxs := make([]*int32, len(s.Suggestions[j].MatchedIndexes))
			for k, idx := range s.Suggestions[j].MatchedIndexes {
				castedIdx := int32(idx)
				idxs[k] = &castedIdx
			}
			as[j] = &AutocompleteSuggestion{
				Kind:           &kind,
				Name:           &s.Suggestions[j].Name,
				Description:    &s.Suggestions[j].Description,
				MatchedIndexes: &idxs,
				State:          &state,
			}
		}

		tabIndex := int32(res.TabSuggestions[i].TabIndex)
		suggestions[i] = &TabSuggestion{
			TabIndex:              &tabIndex,
			ExecutableAfterSelect: &res.TabSuggestions[i].ExecutableAfterSelect,
			Suggestions:           &as,
		}
	}

	return &AutocompleteResolver{
		FormattedInput: &res.FormattedInput,
		IsExecutable:   &res.IsExecutable,
		TabSuggestions: &suggestions,
	}, nil
}

type autocompleteFieldArgs struct {
	Input            *string
	FieldType        *string
	RequiredArgTypes *[]*string
	ClusterUID       *string
}

type autocompleteArgs struct {
	Input      *string
	CursorPos  *int32
	Action     *string
	ClusterUID *string
}

// AutocompleteResolver is the resolver for an autocomplete response.
type AutocompleteResolver struct {
	FormattedInput *string
	IsExecutable   *bool
	TabSuggestions *[]*TabSuggestion
}

// TabSuggestion represents suggestions for a tab index.
type TabSuggestion struct {
	TabIndex              *int32
	ExecutableAfterSelect *bool
	Suggestions           *[]*AutocompleteSuggestion
}

// AutocompleteFieldResolver is the resolver for an autocomplete field response.
type AutocompleteFieldResolver struct {
	Suggestions          []*AutocompleteSuggestion
	HasAdditionalMatches bool
}

// AutocompleteSuggestion represents a single suggestion.
type AutocompleteSuggestion struct {
	Kind           *string
	Name           *string
	Description    *string
	MatchedIndexes *[]*int32
	State          *string
}
