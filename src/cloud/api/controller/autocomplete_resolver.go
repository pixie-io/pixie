package controller

import (
	"context"
)

// Autocomplete responds to an autocomplete request.
func (q *QueryResolver) Autocomplete(ctx context.Context, args *autocompleteArgs) (*AutocompleteResolver, error) {
	return nil, nil
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
	Kind        *string
	Name        *string
	Description *string
}
