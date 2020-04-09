package autocomplete_test

import (
	"testing"

	"github.com/golang/mock/gomock"
	"github.com/stretchr/testify/assert"

	"pixielabs.ai/pixielabs/src/cloud/autocomplete"
	"pixielabs.ai/pixielabs/src/cloud/autocomplete/mock"
	"pixielabs.ai/pixielabs/src/cloud/cloudapipb"
)

type SuggestionRequest struct {
	requestKinds []cloudapipb.AutocompleteEntityKind
	requestArgs  []cloudapipb.AutocompleteEntityKind
	suggestions  []*autocomplete.Suggestion
}

func TestParseIntoCommand(t *testing.T) {
	tests := []struct {
		name        string
		input       string
		requests    map[string]*SuggestionRequest
		expectedCmd *autocomplete.Command
	}{
		{
			name:  "valid",
			input: "script:px/svc_info svc:pl/test",
			requests: map[string]*SuggestionRequest{
				"px/svc_info": &SuggestionRequest{
					requestKinds: []cloudapipb.AutocompleteEntityKind{cloudapipb.AEK_SCRIPT},
					requestArgs:  []cloudapipb.AutocompleteEntityKind{},
					suggestions: []*autocomplete.Suggestion{
						&autocomplete.Suggestion{
							Name:  "px/svc_info",
							Score: 1,
							Args:  []cloudapipb.AutocompleteEntityKind{cloudapipb.AEK_SVC},
						},
					},
				},
				"pl/test": &SuggestionRequest{
					requestKinds: []cloudapipb.AutocompleteEntityKind{cloudapipb.AEK_SVC},
					requestArgs:  []cloudapipb.AutocompleteEntityKind{cloudapipb.AEK_SVC},
					suggestions: []*autocomplete.Suggestion{
						&autocomplete.Suggestion{
							Name:  "pl/test",
							Score: 1,
						},
					},
				},
			},
			expectedCmd: &autocomplete.Command{
				TabStops: []*autocomplete.TabStop{
					&autocomplete.TabStop{
						Value: "run",
						Kind:  cloudapipb.AEK_UNKNOWN,
						Valid: true,
					},
					&autocomplete.TabStop{
						Value: "px/svc_info",
						Kind:  cloudapipb.AEK_SCRIPT,
						Valid: true,
					},
					&autocomplete.TabStop{
						Value: "pl/test",
						Kind:  cloudapipb.AEK_SVC,
						Valid: true,
					},
				},
				Executable: true,
			},
		},
		{
			name:  "invalid",
			input: "script:px/svc_info pod:pl/test",
			requests: map[string]*SuggestionRequest{
				"px/svc_info": &SuggestionRequest{
					requestKinds: []cloudapipb.AutocompleteEntityKind{cloudapipb.AEK_SCRIPT},
					requestArgs:  []cloudapipb.AutocompleteEntityKind{},
					suggestions: []*autocomplete.Suggestion{
						&autocomplete.Suggestion{
							Name:  "px/svc_info",
							Score: 1,
							Args:  []cloudapipb.AutocompleteEntityKind{cloudapipb.AEK_SVC},
						},
					},
				},
				"pod:pl/test": &SuggestionRequest{
					requestKinds: []cloudapipb.AutocompleteEntityKind{cloudapipb.AEK_SVC},
					requestArgs:  []cloudapipb.AutocompleteEntityKind{},
					suggestions: []*autocomplete.Suggestion{
						&autocomplete.Suggestion{
							Name:  "pl/test",
							Score: 1,
						},
					},
				},
			},
			expectedCmd: &autocomplete.Command{
				TabStops: []*autocomplete.TabStop{
					&autocomplete.TabStop{
						Value: "run",
						Kind:  cloudapipb.AEK_UNKNOWN,
						Valid: true,
					},
					&autocomplete.TabStop{
						Value: "px/svc_info",
						Kind:  cloudapipb.AEK_SCRIPT,
						Valid: true,
					},
					&autocomplete.TabStop{
						Value: "pod:pl/test",
						Kind:  cloudapipb.AEK_UNKNOWN,
						Valid: false,
					},
				},
				Executable: false,
			},
		},
		{
			name:  "no script, defined entity",
			input: "px/svc_info pod:pl/test",
			requests: map[string]*SuggestionRequest{
				"px/svc_info": &SuggestionRequest{
					requestKinds: []cloudapipb.AutocompleteEntityKind{cloudapipb.AEK_POD, cloudapipb.AEK_SVC, cloudapipb.AEK_NAMESPACE, cloudapipb.AEK_SCRIPT},
					requestArgs:  []cloudapipb.AutocompleteEntityKind{cloudapipb.AEK_POD},
					suggestions: []*autocomplete.Suggestion{
						&autocomplete.Suggestion{
							Name:  "px/svc_info",
							Score: 1,
							Args:  []cloudapipb.AutocompleteEntityKind{cloudapipb.AEK_SVC},
						},
					},
				},
				"pl/test": &SuggestionRequest{
					requestKinds: []cloudapipb.AutocompleteEntityKind{cloudapipb.AEK_POD},
					requestArgs:  []cloudapipb.AutocompleteEntityKind{cloudapipb.AEK_POD},
					suggestions: []*autocomplete.Suggestion{
						&autocomplete.Suggestion{
							Name:  "pl/test",
							Score: 1,
						},
					},
				},
			},
			expectedCmd: &autocomplete.Command{
				TabStops: []*autocomplete.TabStop{
					&autocomplete.TabStop{
						Value: "run",
						Kind:  cloudapipb.AEK_UNKNOWN,
						Valid: true,
					},
					&autocomplete.TabStop{
						Value: "px/svc_info",
						Kind:  cloudapipb.AEK_UNKNOWN,
						Valid: false,
					},
					&autocomplete.TabStop{
						Value: "pl/test",
						Kind:  cloudapipb.AEK_POD,
						Valid: true,
					},
				},
				Executable: false,
			},
		},
		{
			name:  "invalid script",
			input: "script:px/svc_info pl/test",
			requests: map[string]*SuggestionRequest{
				"px/svc_info": &SuggestionRequest{
					requestKinds: []cloudapipb.AutocompleteEntityKind{cloudapipb.AEK_SCRIPT},
					requestArgs:  []cloudapipb.AutocompleteEntityKind{},
					suggestions: []*autocomplete.Suggestion{
						&autocomplete.Suggestion{
							Name:  "px/svc_infos",
							Score: 0.5,
							Args:  []cloudapipb.AutocompleteEntityKind{cloudapipb.AEK_SVC},
						},
					},
				},
				"pl/test": &SuggestionRequest{
					requestKinds: []cloudapipb.AutocompleteEntityKind{cloudapipb.AEK_POD, cloudapipb.AEK_SVC, cloudapipb.AEK_NAMESPACE},
					requestArgs:  []cloudapipb.AutocompleteEntityKind{},
					suggestions: []*autocomplete.Suggestion{
						&autocomplete.Suggestion{
							Name:  "pl/test",
							Score: 1,
						},
					},
				},
			},
			expectedCmd: &autocomplete.Command{
				TabStops: []*autocomplete.TabStop{
					&autocomplete.TabStop{
						Value: "run",
						Kind:  cloudapipb.AEK_UNKNOWN,
						Valid: true,
					},
					&autocomplete.TabStop{
						Value: "px/svc_info",
						Kind:  cloudapipb.AEK_SCRIPT,
						Valid: false,
					},
					&autocomplete.TabStop{
						Value: "pl/test",
						Kind:  cloudapipb.AEK_UNKNOWN,
						Valid: false,
					},
				},
				Executable: false,
			},
		},
		{
			name:  "script with two args",
			input: "script:px/svc_info svc:pl/test test",
			requests: map[string]*SuggestionRequest{
				"px/svc_info": &SuggestionRequest{
					requestKinds: []cloudapipb.AutocompleteEntityKind{cloudapipb.AEK_SCRIPT},
					requestArgs:  []cloudapipb.AutocompleteEntityKind{},
					suggestions: []*autocomplete.Suggestion{
						&autocomplete.Suggestion{
							Name:  "px/svc_info",
							Score: 1,
							Args:  []cloudapipb.AutocompleteEntityKind{cloudapipb.AEK_SVC, cloudapipb.AEK_SVC},
						},
					},
				},
				"pl/test": &SuggestionRequest{
					requestKinds: []cloudapipb.AutocompleteEntityKind{cloudapipb.AEK_SVC},
					requestArgs:  []cloudapipb.AutocompleteEntityKind{cloudapipb.AEK_SVC},
					suggestions: []*autocomplete.Suggestion{
						&autocomplete.Suggestion{
							Name:  "pl/test",
							Score: 1,
						},
					},
				},
				"test": &SuggestionRequest{
					requestKinds: []cloudapipb.AutocompleteEntityKind{cloudapipb.AEK_SVC},
					requestArgs:  []cloudapipb.AutocompleteEntityKind{cloudapipb.AEK_SVC},
					suggestions: []*autocomplete.Suggestion{
						&autocomplete.Suggestion{
							Name:  "pl/test",
							Score: 0.5,
						},
					},
				},
			},
			expectedCmd: &autocomplete.Command{
				TabStops: []*autocomplete.TabStop{
					&autocomplete.TabStop{
						Value: "run",
						Kind:  cloudapipb.AEK_UNKNOWN,
						Valid: true,
					},
					&autocomplete.TabStop{
						Value: "px/svc_info",
						Kind:  cloudapipb.AEK_SCRIPT,
						Valid: true,
					},
					&autocomplete.TabStop{
						Value: "pl/test",
						Kind:  cloudapipb.AEK_SVC,
						Valid: true,
					},
					&autocomplete.TabStop{
						Value: "test",
						Kind:  cloudapipb.AEK_UNKNOWN,
						Valid: false,
					},
				},
				Executable: false,
			},
		},
		{
			name:  "invalid label",
			input: "script:px/svc_info no:pl/test",
			requests: map[string]*SuggestionRequest{
				"px/svc_info": &SuggestionRequest{
					requestKinds: []cloudapipb.AutocompleteEntityKind{cloudapipb.AEK_SCRIPT},
					requestArgs:  []cloudapipb.AutocompleteEntityKind{},
					suggestions: []*autocomplete.Suggestion{
						&autocomplete.Suggestion{
							Name:  "px/svc_info",
							Score: 1,
							Args:  []cloudapipb.AutocompleteEntityKind{cloudapipb.AEK_SVC},
						},
					},
				},
				"no:pl/test": &SuggestionRequest{
					requestKinds: []cloudapipb.AutocompleteEntityKind{cloudapipb.AEK_SVC},
					requestArgs:  []cloudapipb.AutocompleteEntityKind{},
					suggestions: []*autocomplete.Suggestion{
						&autocomplete.Suggestion{
							Name:  "pl/test",
							Score: 0.5,
						},
					},
				},
			},
			expectedCmd: &autocomplete.Command{
				TabStops: []*autocomplete.TabStop{
					&autocomplete.TabStop{
						Value: "run",
						Kind:  cloudapipb.AEK_UNKNOWN,
						Valid: true,
					},
					&autocomplete.TabStop{
						Value: "px/svc_info",
						Kind:  cloudapipb.AEK_SCRIPT,
						Valid: true,
					},
					&autocomplete.TabStop{
						Value: "no:pl/test",
						Kind:  cloudapipb.AEK_UNKNOWN,
						Valid: false,
					},
				},
				Executable: false,
			},
		},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			ctrl := gomock.NewController(t)
			defer ctrl.Finish()
			s := mock_autocomplete.NewMockSuggester(ctrl)
			for k, v := range test.requests {
				suggestions := v.suggestions
				exactMatch := false
				if len(suggestions) > 0 {
					exactMatch = suggestions[0].Score == 1
				}
				s.
					EXPECT().
					GetSuggestions(k, v.requestKinds, v.requestArgs).
					Return(suggestions, exactMatch, nil)
			}

			cmd, err := autocomplete.ParseIntoCommand(test.input, s)
			assert.Nil(t, err)
			assert.NotNil(t, cmd)

			assert.Equal(t, test.expectedCmd.Executable, cmd.Executable)
			assert.Equal(t, len(test.expectedCmd.TabStops), len(cmd.TabStops))
			for i, a := range test.expectedCmd.TabStops {
				assert.Equal(t, a.Value, cmd.TabStops[i].Value)
				assert.Equal(t, a.Valid, cmd.TabStops[i].Valid)
				assert.Equal(t, a.Kind, cmd.TabStops[i].Kind)
			}
		})
	}
}
