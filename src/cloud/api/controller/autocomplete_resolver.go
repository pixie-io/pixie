package controller

import (
	"context"

	"pixielabs.ai/pixielabs/src/cloud/cloudapipb"
)

var actionToProtoMap = map[string]cloudapipb.AutocompleteActionType{
	"AAT_UNKNOWN": cloudapipb.AAT_UNKNOWN,
	"AAT_EDIT":    cloudapipb.AAT_EDIT,
	"AAT_SELECT":  cloudapipb.AAT_SELECT,
}

var protoToKindMap = map[cloudapipb.AutocompleteEntityKind]string{
	cloudapipb.AEK_UNKNOWN:   "AEK_UNKNOWN",
	cloudapipb.AEK_POD:       "AEK_POD",
	cloudapipb.AEK_SVC:       "AEK_SVC",
	cloudapipb.AEK_SCRIPT:    "AEK_SCRIPT",
	cloudapipb.AEK_NAMESPACE: "AEK_NAMESPACE",
}

// Autocomplete responds to an autocomplete request.
func (q *QueryResolver) Autocomplete(ctx context.Context, args *autocompleteArgs) (*AutocompleteResolver, error) {
	grpcAPI := q.Env.AutocompleteServer
	res, err := grpcAPI.Autocomplete(ctx, &cloudapipb.AutocompleteRequest{
		Input:     *args.Input,
		CursorPos: int64(*args.CursorPos),
		Action:    actionToProtoMap[*args.Action],
	})
	if err != nil {
		return nil, err
	}

	suggestions := make([]*TabSuggestion, len(res.TabSuggestions))
	for i, s := range res.TabSuggestions {
		as := make([]*AutocompleteSuggestion, len(s.Suggestions))
		for j := range s.Suggestions {
			kind := protoToKindMap[s.Suggestions[j].Kind]
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

type autocompleteArgs struct {
	Input     *string
	CursorPos *int32
	Action    *string
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

// AutocompleteSuggestion represents a single suggestion.
type AutocompleteSuggestion struct {
	Kind           *string
	Name           *string
	Description    *string
	MatchedIndexes *[]*int32
}
