package autocomplete_test

import (
	"testing"

	"github.com/gofrs/uuid"
	"github.com/golang/mock/gomock"
	"github.com/stretchr/testify/assert"

	"pixielabs.ai/pixielabs/src/cloud/autocomplete"
	mock_autocomplete "pixielabs.ai/pixielabs/src/cloud/autocomplete/mock"
	"pixielabs.ai/pixielabs/src/cloud/cloudapipb"
)

var orgID = uuid.Must(uuid.NewV4())

func TestParseIntoCommand(t *testing.T) {
	tests := []struct {
		name        string
		input       string
		requests    [][]*autocomplete.SuggestionRequest
		responses   [][]*autocomplete.SuggestionResult
		expectedCmd *autocomplete.Command
	}{
		{
			name:  "valid",
			input: "script:px/svc_info svc_name:pl/test",
			requests: [][]*autocomplete.SuggestionRequest{
				{
					{
						OrgID:        orgID,
						ClusterUID:   "test",
						Input:        "px/svc_info",
						AllowedKinds: []cloudapipb.AutocompleteEntityKind{cloudapipb.AEK_SCRIPT},
						AllowedArgs:  []cloudapipb.AutocompleteEntityKind{},
					},
				},
				{
					{
						OrgID:        orgID,
						ClusterUID:   "test",
						Input:        "pl/test",
						AllowedKinds: []cloudapipb.AutocompleteEntityKind{cloudapipb.AEK_SVC},
						AllowedArgs:  []cloudapipb.AutocompleteEntityKind{},
					},
				},
			},
			responses: [][]*autocomplete.SuggestionResult{
				{
					{
						Suggestions: []*autocomplete.Suggestion{
							{
								Name:     "px/svc_info",
								Score:    1,
								ArgNames: []string{"svc_name"},
								ArgKinds: []cloudapipb.AutocompleteEntityKind{cloudapipb.AEK_SVC},
							},
						},
						ExactMatch: true,
					},
				},
				{
					{
						Suggestions: []*autocomplete.Suggestion{
							{
								Name:  "px/test",
								Score: 1,
							},
						},
						ExactMatch: true,
					},
				},
			},
			expectedCmd: &autocomplete.Command{
				TabStops: []*autocomplete.TabStop{
					{
						Value: "px/svc_info",
						Kind:  cloudapipb.AEK_SCRIPT,
						Valid: true,
					},
					{
						Value:   "pl/test",
						Kind:    cloudapipb.AEK_SVC,
						Valid:   true,
						ArgName: "svc_name",
					},
				},
				Executable: true,
			},
		},
		{
			name:  "valid with run",
			input: "run script:px/svc_info svc_name:pl/test",
			requests: [][]*autocomplete.SuggestionRequest{
				{
					{
						OrgID:        orgID,
						ClusterUID:   "test",
						Input:        "px/svc_info",
						AllowedKinds: []cloudapipb.AutocompleteEntityKind{cloudapipb.AEK_SCRIPT},
						AllowedArgs:  []cloudapipb.AutocompleteEntityKind{},
					},
				},
				{
					{
						OrgID:        orgID,
						ClusterUID:   "test",
						Input:        "pl/test",
						AllowedKinds: []cloudapipb.AutocompleteEntityKind{cloudapipb.AEK_SVC},
						AllowedArgs:  []cloudapipb.AutocompleteEntityKind{},
					},
				},
			},
			responses: [][]*autocomplete.SuggestionResult{
				{
					{
						Suggestions: []*autocomplete.Suggestion{
							{
								Name:     "px/svc_info",
								Score:    1,
								ArgNames: []string{"svc_name"},
								ArgKinds: []cloudapipb.AutocompleteEntityKind{cloudapipb.AEK_SVC},
							},
						},
						ExactMatch: true,
					},
				},
				{
					{
						Suggestions: []*autocomplete.Suggestion{
							{
								Name:  "px/test",
								Score: 1,
							},
						},
						ExactMatch: true,
					},
				},
			},
			expectedCmd: &autocomplete.Command{
				TabStops: []*autocomplete.TabStop{
					{
						Value: "run",
						Kind:  cloudapipb.AEK_UNKNOWN,
						Valid: true,
					},
					{
						Value: "px/svc_info",
						Kind:  cloudapipb.AEK_SCRIPT,
						Valid: true,
					},
					{
						Value:   "pl/test",
						Kind:    cloudapipb.AEK_SVC,
						Valid:   true,
						ArgName: "svc_name",
					},
				},
				Executable: true,
			},
		},
		{
			name:  "script with entity defined",
			input: "script:px/svc_info svc:pl/test",
			requests: [][]*autocomplete.SuggestionRequest{
				{
					{
						OrgID:        orgID,
						ClusterUID:   "test",
						Input:        "px/svc_info",
						AllowedKinds: []cloudapipb.AutocompleteEntityKind{cloudapipb.AEK_SCRIPT},
						AllowedArgs:  []cloudapipb.AutocompleteEntityKind{},
					},
				},
				{
					{
						OrgID:        orgID,
						ClusterUID:   "test",
						Input:        "pl/test",
						AllowedKinds: []cloudapipb.AutocompleteEntityKind{cloudapipb.AEK_SVC},
						AllowedArgs:  []cloudapipb.AutocompleteEntityKind{},
					},
				},
			},
			responses: [][]*autocomplete.SuggestionResult{
				{
					{
						Suggestions: []*autocomplete.Suggestion{
							{
								Name:     "px/svc_info",
								Score:    1,
								ArgNames: []string{"svc_name"},
								ArgKinds: []cloudapipb.AutocompleteEntityKind{cloudapipb.AEK_SVC},
							},
						},
						ExactMatch: true,
					},
				},
				{
					{
						Suggestions: []*autocomplete.Suggestion{
							{
								Name:  "pl/test",
								Score: 1,
							},
						},
						ExactMatch: true,
					},
				},
			},
			expectedCmd: &autocomplete.Command{
				TabStops: []*autocomplete.TabStop{
					{
						Value: "px/svc_info",
						Kind:  cloudapipb.AEK_SCRIPT,
						Valid: true,
					},
					{
						Value:   "pl/test",
						Kind:    cloudapipb.AEK_SVC,
						Valid:   true,
						ArgName: "svc_name",
					},
				},
				Executable: true,
			},
		},
		{
			name:  "invalid",
			input: "script:px/svc_info pod:pl/test",
			requests: [][]*autocomplete.SuggestionRequest{
				{
					{
						OrgID:        orgID,
						ClusterUID:   "test",
						Input:        "px/svc_info",
						AllowedKinds: []cloudapipb.AutocompleteEntityKind{cloudapipb.AEK_SCRIPT},
						AllowedArgs:  []cloudapipb.AutocompleteEntityKind{},
					},
				},
				{
					{
						OrgID:        orgID,
						ClusterUID:   "test",
						Input:        "pl/test",
						AllowedKinds: []cloudapipb.AutocompleteEntityKind{cloudapipb.AEK_SVC},
						AllowedArgs:  []cloudapipb.AutocompleteEntityKind{},
					},
				},
			},
			responses: [][]*autocomplete.SuggestionResult{
				{
					{
						Suggestions: []*autocomplete.Suggestion{
							{
								Name:     "px/svc_info",
								Score:    1,
								ArgNames: []string{"svc_name"},
								ArgKinds: []cloudapipb.AutocompleteEntityKind{cloudapipb.AEK_SVC},
							},
						},
						ExactMatch: true,
					},
				},
				{
					{
						Suggestions: []*autocomplete.Suggestion{
							{
								Name:  "pl/test",
								Score: 1,
							},
						},
						ExactMatch: false,
					},
				},
			},
			expectedCmd: &autocomplete.Command{
				TabStops: []*autocomplete.TabStop{
					{
						Value: "px/svc_info",
						Kind:  cloudapipb.AEK_SCRIPT,
						Valid: true,
					},
					{
						Value:   "pl/test",
						Kind:    cloudapipb.AEK_SVC,
						Valid:   false,
						ArgName: "svc_name",
					},
				},
				Executable: false,
			},
		},
		{
			name:  "no script, defined entity",
			input: "px/svc_info pod:pl/test",
			requests: [][]*autocomplete.SuggestionRequest{
				{
					{
						OrgID:        orgID,
						ClusterUID:   "test",
						Input:        "px/svc_info",
						AllowedKinds: []cloudapipb.AutocompleteEntityKind{cloudapipb.AEK_POD, cloudapipb.AEK_SVC, cloudapipb.AEK_NAMESPACE, cloudapipb.AEK_SCRIPT},
						AllowedArgs:  []cloudapipb.AutocompleteEntityKind{cloudapipb.AEK_POD},
					},
					{
						OrgID:        orgID,
						ClusterUID:   "test",
						Input:        "pl/test",
						AllowedKinds: []cloudapipb.AutocompleteEntityKind{cloudapipb.AEK_POD},
						AllowedArgs:  []cloudapipb.AutocompleteEntityKind{cloudapipb.AEK_POD},
					},
				},
			},
			responses: [][]*autocomplete.SuggestionResult{
				{
					{
						Suggestions: []*autocomplete.Suggestion{
							{
								Name:     "px/svc_info",
								Score:    1,
								ArgNames: []string{"svc_name"},
								ArgKinds: []cloudapipb.AutocompleteEntityKind{cloudapipb.AEK_SVC},
							},
						},
						ExactMatch: false,
					},
					{
						Suggestions: []*autocomplete.Suggestion{
							{
								Name:  "pl/test",
								Score: 1,
							},
						},
						ExactMatch: true,
					},
				},
			},
			expectedCmd: &autocomplete.Command{
				TabStops: []*autocomplete.TabStop{
					{
						Value: "px/svc_info",
						Kind:  cloudapipb.AEK_UNKNOWN,
						Valid: false,
					},
					{
						Value: "pl/test",
						Kind:  cloudapipb.AEK_POD,
						Valid: false,
					},
				},
				Executable: false,
			},
		},
		{
			name:  "invalid script",
			input: "script:px/svc_info pl/$0test",
			requests: [][]*autocomplete.SuggestionRequest{
				{
					{
						OrgID:        orgID,
						ClusterUID:   "test",
						Input:        "px/svc_info",
						AllowedKinds: []cloudapipb.AutocompleteEntityKind{cloudapipb.AEK_SCRIPT},
						AllowedArgs:  []cloudapipb.AutocompleteEntityKind{},
					},
				},
				{
					{
						OrgID:        orgID,
						ClusterUID:   "test",
						Input:        "pl/test",
						AllowedKinds: []cloudapipb.AutocompleteEntityKind{cloudapipb.AEK_POD, cloudapipb.AEK_SVC, cloudapipb.AEK_NAMESPACE},
						AllowedArgs:  []cloudapipb.AutocompleteEntityKind{},
					},
				},
			},
			responses: [][]*autocomplete.SuggestionResult{
				{
					{
						Suggestions: []*autocomplete.Suggestion{
							{
								Name:     "px/svc_infos",
								Score:    1,
								ArgNames: []string{"svc_name"},
								ArgKinds: []cloudapipb.AutocompleteEntityKind{cloudapipb.AEK_SVC},
							},
						},
						ExactMatch: false,
					},
				},
				{
					{
						Suggestions: []*autocomplete.Suggestion{
							{
								Name:  "pl/test",
								Score: 1,
							},
						},
						ExactMatch: true,
					},
				},
			},
			expectedCmd: &autocomplete.Command{
				TabStops: []*autocomplete.TabStop{
					{
						Value: "px/svc_info",
						Kind:  cloudapipb.AEK_SCRIPT,
						Valid: false,
					},
					{
						Value:          "pl/$0test",
						Kind:           cloudapipb.AEK_UNKNOWN,
						Valid:          false,
						ContainsCursor: true,
					},
				},
				Executable: false,
			},
		},
		{
			name:  "script with two args",
			input: "script:$0px/svc_info svc:pl/test test",
			requests: [][]*autocomplete.SuggestionRequest{
				{
					{
						OrgID:        orgID,
						ClusterUID:   "test",
						Input:        "px/svc_info",
						AllowedKinds: []cloudapipb.AutocompleteEntityKind{cloudapipb.AEK_SCRIPT},
						AllowedArgs:  []cloudapipb.AutocompleteEntityKind{},
					},
				},
				{
					{
						OrgID:        orgID,
						ClusterUID:   "test",
						Input:        "pl/test",
						AllowedKinds: []cloudapipb.AutocompleteEntityKind{cloudapipb.AEK_SVC},
						AllowedArgs:  []cloudapipb.AutocompleteEntityKind{},
					},
					{
						OrgID:        orgID,
						ClusterUID:   "test",
						Input:        "test",
						AllowedKinds: []cloudapipb.AutocompleteEntityKind{cloudapipb.AEK_SVC},
						AllowedArgs:  []cloudapipb.AutocompleteEntityKind{},
					},
				},
			},
			responses: [][]*autocomplete.SuggestionResult{
				{
					{
						Suggestions: []*autocomplete.Suggestion{
							{
								Name:     "px/svc_info",
								Score:    1,
								ArgNames: []string{"svc_name", "svc_name2"},
								ArgKinds: []cloudapipb.AutocompleteEntityKind{cloudapipb.AEK_SVC, cloudapipb.AEK_SVC},
							},
						},
						ExactMatch: true,
					},
				},
				{
					{
						Suggestions: []*autocomplete.Suggestion{
							{
								Name:  "pl/test",
								Score: 1,
							},
						},
						ExactMatch: true,
					},
					{
						Suggestions: []*autocomplete.Suggestion{
							{
								Name:  "pl/test",
								Score: 1,
							},
						},
						ExactMatch: false,
					},
				},
			},
			expectedCmd: &autocomplete.Command{
				TabStops: []*autocomplete.TabStop{
					{
						Value:          "$0px/svc_info",
						Kind:           cloudapipb.AEK_SCRIPT,
						Valid:          true,
						ContainsCursor: true,
					},
					{
						Value:   "pl/test",
						Kind:    cloudapipb.AEK_SVC,
						Valid:   true,
						ArgName: "svc_name",
					},
					{
						Value:   "test",
						Kind:    cloudapipb.AEK_SVC,
						ArgName: "svc_name2",
						Valid:   false,
					},
				},
				Executable: false,
			},
		},
		{
			name:  "invalid label",
			input: "script:px/svc_info no:pl/test",
			requests: [][]*autocomplete.SuggestionRequest{
				{
					{
						OrgID:        orgID,
						ClusterUID:   "test",
						Input:        "px/svc_info",
						AllowedKinds: []cloudapipb.AutocompleteEntityKind{cloudapipb.AEK_SCRIPT},
						AllowedArgs:  []cloudapipb.AutocompleteEntityKind{},
					},
				},
				{
					{
						OrgID:        orgID,
						ClusterUID:   "test",
						Input:        "pl/test",
						AllowedKinds: []cloudapipb.AutocompleteEntityKind{cloudapipb.AEK_SVC},
						AllowedArgs:  []cloudapipb.AutocompleteEntityKind{},
					},
					{
						OrgID:        orgID,
						ClusterUID:   "test",
						Input:        "",
						AllowedKinds: []cloudapipb.AutocompleteEntityKind{cloudapipb.AEK_SVC},
						AllowedArgs:  []cloudapipb.AutocompleteEntityKind{},
					},
				},
			},
			responses: [][]*autocomplete.SuggestionResult{
				{
					{
						Suggestions: []*autocomplete.Suggestion{
							{
								Name:     "px/svc_info",
								Score:    1,
								ArgNames: []string{"svc_name", "svc_name2"},
								ArgKinds: []cloudapipb.AutocompleteEntityKind{cloudapipb.AEK_SVC, cloudapipb.AEK_SVC},
							},
						},
						ExactMatch: true,
					},
				},
				{
					{
						Suggestions: []*autocomplete.Suggestion{
							{
								Name:  "pl/test",
								Score: 1,
							},
						},
						ExactMatch: false,
					},
					{
						Suggestions: []*autocomplete.Suggestion{
							{
								Name:  "pl/some_svc",
								Score: 0,
							},
						},
						ExactMatch: false,
					},
				},
			},
			expectedCmd: &autocomplete.Command{
				TabStops: []*autocomplete.TabStop{
					{
						Value: "px/svc_info",
						Kind:  cloudapipb.AEK_SCRIPT,
						Valid: true,
					},
					{
						Value:   "pl/test",
						Kind:    cloudapipb.AEK_SVC,
						Valid:   false,
						ArgName: "svc_name",
					},
					{
						Value:   "",
						Kind:    cloudapipb.AEK_SVC,
						Valid:   false,
						ArgName: "svc_name2",
					},
				},
				Executable: false,
			},
		},
		{
			name:  "no args",
			input: "script:px/svc_info",
			requests: [][]*autocomplete.SuggestionRequest{
				{
					{
						OrgID:        orgID,
						ClusterUID:   "test",
						Input:        "px/svc_info",
						AllowedKinds: []cloudapipb.AutocompleteEntityKind{cloudapipb.AEK_SCRIPT},
						AllowedArgs:  []cloudapipb.AutocompleteEntityKind{},
					},
				},
				{
					{
						OrgID:        orgID,
						ClusterUID:   "test",
						Input:        "",
						AllowedKinds: []cloudapipb.AutocompleteEntityKind{cloudapipb.AEK_SVC},
						AllowedArgs:  []cloudapipb.AutocompleteEntityKind{},
					},
					{
						OrgID:        orgID,
						ClusterUID:   "test",
						Input:        "",
						AllowedKinds: []cloudapipb.AutocompleteEntityKind{cloudapipb.AEK_SVC},
						AllowedArgs:  []cloudapipb.AutocompleteEntityKind{},
					},
				},
			},
			responses: [][]*autocomplete.SuggestionResult{
				{
					{
						Suggestions: []*autocomplete.Suggestion{
							{
								Name:     "px/svc_info",
								Score:    1,
								ArgNames: []string{"svc_name", "svc_name2"},
								ArgKinds: []cloudapipb.AutocompleteEntityKind{cloudapipb.AEK_SVC, cloudapipb.AEK_SVC},
							},
						},
						ExactMatch: true,
					},
				},
				{
					{
						Suggestions: []*autocomplete.Suggestion{
							{
								Name:  "pl/test",
								Score: 1,
							},
						},
						ExactMatch: false,
					},
					{
						Suggestions: []*autocomplete.Suggestion{
							{
								Name:  "pl/some_svc",
								Score: 0,
							},
						},
						ExactMatch: false,
					},
				},
			},
			expectedCmd: &autocomplete.Command{
				TabStops: []*autocomplete.TabStop{
					{
						Value: "px/svc_info",
						Kind:  cloudapipb.AEK_SCRIPT,
						Valid: true,
					},
					{
						Value:   "",
						Kind:    cloudapipb.AEK_SVC,
						Valid:   false,
						ArgName: "svc_name",
					},
					{
						Value:   "",
						Kind:    cloudapipb.AEK_SVC,
						Valid:   false,
						ArgName: "svc_name2",
					},
				},
				Executable: true,
			},
		},
		{
			name:  "extra arg",
			input: "script:px/svc_info svc_name:pl/test test",
			requests: [][]*autocomplete.SuggestionRequest{
				{
					{
						OrgID:        orgID,
						ClusterUID:   "test",
						Input:        "px/svc_info",
						AllowedKinds: []cloudapipb.AutocompleteEntityKind{cloudapipb.AEK_SCRIPT},
						AllowedArgs:  []cloudapipb.AutocompleteEntityKind{},
					},
				},
				{
					{
						OrgID:        orgID,
						ClusterUID:   "test",
						Input:        "pl/test",
						AllowedKinds: []cloudapipb.AutocompleteEntityKind{cloudapipb.AEK_SVC},
						AllowedArgs:  []cloudapipb.AutocompleteEntityKind{},
					},
					{
						OrgID:        orgID,
						ClusterUID:   "test",
						Input:        "test",
						AllowedKinds: []cloudapipb.AutocompleteEntityKind{},
						AllowedArgs:  []cloudapipb.AutocompleteEntityKind{},
					},
				},
			},
			responses: [][]*autocomplete.SuggestionResult{
				{
					{
						Suggestions: []*autocomplete.Suggestion{
							{
								Name:     "px/svc_info",
								Score:    1,
								ArgNames: []string{"svc_name"},
								ArgKinds: []cloudapipb.AutocompleteEntityKind{cloudapipb.AEK_SVC},
							},
						},
						ExactMatch: true,
					},
				},
				{
					{
						Suggestions: []*autocomplete.Suggestion{
							{
								Name:  "pl/test",
								Score: 1,
							},
						},
						ExactMatch: true,
					},
					{
						Suggestions: []*autocomplete.Suggestion{},
						ExactMatch:  false,
					},
				},
			},
			expectedCmd: &autocomplete.Command{
				TabStops: []*autocomplete.TabStop{
					{
						Value: "px/svc_info",
						Kind:  cloudapipb.AEK_SCRIPT,
						Valid: true,
					},
					{
						Value:   "pl/test",
						Kind:    cloudapipb.AEK_SVC,
						Valid:   true,
						ArgName: "svc_name",
					},
					{
						Value: "test",
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
			suggestionCalls := 0

			s.EXPECT().
				GetSuggestions(gomock.Any()).
				DoAndReturn(func(req []*autocomplete.SuggestionRequest) ([]*autocomplete.SuggestionResult, error) {
					assert.ElementsMatch(t, test.requests[suggestionCalls], req)
					resp := test.responses[suggestionCalls]
					suggestionCalls++

					return resp, nil
				}).
				Times(len(test.requests))

			cmd, err := autocomplete.ParseIntoCommand(test.input, s, orgID, "test")
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

func TestToFormatString(t *testing.T) {
	tests := []struct {
		name                  string
		cmd                   *autocomplete.Command
		action                cloudapipb.AutocompleteActionType
		expectedStr           string
		expectedSuggestions   []*cloudapipb.TabSuggestion
		callSuggester         bool
		suggestionScriptTypes []cloudapipb.AutocompleteEntityKind
	}{
		{
			name: "edit",
			cmd: &autocomplete.Command{
				TabStops: []*autocomplete.TabStop{
					{
						Value: "run",
						Kind:  cloudapipb.AEK_UNKNOWN,
						Valid: true,
					},
					{
						Value:          "px/$0svc_info",
						Kind:           cloudapipb.AEK_SCRIPT,
						Valid:          true,
						ContainsCursor: true,
					},
					{
						Value: "pl/test",
						Kind:  cloudapipb.AEK_UNKNOWN,
						Valid: false,
						Suggestions: []*autocomplete.Suggestion{
							{
								Name:  "pl/test",
								Desc:  "a svc",
								Kind:  cloudapipb.AEK_SVC,
								State: cloudapipb.AES_RUNNING,
							},
						},
					},
				},
				Executable: false,
			},
			action:      cloudapipb.AAT_EDIT,
			expectedStr: "${2:run} ${3:script:px/$0svc_info} ${1:pl/test}",
			expectedSuggestions: []*cloudapipb.TabSuggestion{
				{
					TabIndex:              2,
					ExecutableAfterSelect: false,
					Suggestions:           []*cloudapipb.AutocompleteSuggestion{},
				},
				{
					TabIndex:              3,
					ExecutableAfterSelect: false,
					Suggestions:           []*cloudapipb.AutocompleteSuggestion{},
				},
				{
					TabIndex:              1,
					ExecutableAfterSelect: true,
					Suggestions: []*cloudapipb.AutocompleteSuggestion{
						{
							Kind:        cloudapipb.AEK_SVC,
							Name:        "pl/test",
							Description: "a svc",
							State:       cloudapipb.AES_RUNNING,
						},
					},
				},
			},
		},
		{
			name: "edit",
			cmd: &autocomplete.Command{
				TabStops: []*autocomplete.TabStop{
					{
						Value: "run",
						Kind:  cloudapipb.AEK_UNKNOWN,
						Valid: true,
					},
					{
						Value:          "px/$0svc_info",
						Kind:           cloudapipb.AEK_SCRIPT,
						Valid:          true,
						ContainsCursor: true,
					},
					{
						Value: "pl/test",
						Kind:  cloudapipb.AEK_UNKNOWN,
						Valid: false,
						Suggestions: []*autocomplete.Suggestion{
							{
								Name: "pl/test",
								Desc: "a svc",
								Kind: cloudapipb.AEK_SVC,
							},
						},
					},
				},
				Executable: false,
			},
			action:      cloudapipb.AAT_SELECT,
			expectedStr: "${1:run} ${2:script:px/svc_info} ${3:pl/test$0}",
			expectedSuggestions: []*cloudapipb.TabSuggestion{
				{
					TabIndex:              1,
					ExecutableAfterSelect: false,
					Suggestions:           []*cloudapipb.AutocompleteSuggestion{},
				},
				{
					TabIndex:              2,
					ExecutableAfterSelect: false,
					Suggestions:           []*cloudapipb.AutocompleteSuggestion{},
				},
				{
					TabIndex:              3,
					ExecutableAfterSelect: true,
					Suggestions: []*cloudapipb.AutocompleteSuggestion{
						{
							Kind:        cloudapipb.AEK_SVC,
							Name:        "pl/test",
							Description: "a svc",
						},
					},
				},
			},
		},
		{
			name: "empty value",
			cmd: &autocomplete.Command{
				TabStops: []*autocomplete.TabStop{
					{
						Value: "run",
						Kind:  cloudapipb.AEK_UNKNOWN,
						Valid: true,
					},
					{
						Value:          "px/svc_info$0",
						Kind:           cloudapipb.AEK_SCRIPT,
						Valid:          true,
						ContainsCursor: true,
					},
					{
						Value: "",
						Kind:  cloudapipb.AEK_UNKNOWN,
						Valid: false,
						Suggestions: []*autocomplete.Suggestion{
							{
								Name: "pl/test",
								Desc: "a svc",
								Kind: cloudapipb.AEK_SVC,
							},
						},
					},
				},
				Executable: false,
			},
			action:      cloudapipb.AAT_EDIT,
			expectedStr: "${2:run} ${3:script:px/svc_info$0} ${1}",
			expectedSuggestions: []*cloudapipb.TabSuggestion{
				{
					TabIndex:              2,
					ExecutableAfterSelect: false,
					Suggestions:           []*cloudapipb.AutocompleteSuggestion{},
				},
				{
					TabIndex:              3,
					ExecutableAfterSelect: false,
					Suggestions:           []*cloudapipb.AutocompleteSuggestion{},
				},
				{
					TabIndex:              1,
					ExecutableAfterSelect: true,
					Suggestions: []*cloudapipb.AutocompleteSuggestion{
						{
							Kind:        cloudapipb.AEK_SVC,
							Name:        "pl/test",
							Description: "a svc",
						},
					},
				},
			},
		},
		{
			name: "invalid before current cursor",
			cmd: &autocomplete.Command{
				TabStops: []*autocomplete.TabStop{
					{
						Value: "run",
						Kind:  cloudapipb.AEK_UNKNOWN,
						Valid: true,
					},
					{
						Value: "px/service_stats",
						Kind:  cloudapipb.AEK_SCRIPT,
						Valid: true,
						Suggestions: []*autocomplete.Suggestion{
							{
								Name: "px/service_stats",
								Desc: "a svc",
								Kind: cloudapipb.AEK_SCRIPT,
							},
						},
					},
					{
						Value:          "pl/",
						Kind:           cloudapipb.AEK_SVC,
						Valid:          false,
						ContainsCursor: false,
						Suggestions: []*autocomplete.Suggestion{
							{
								Name: "pl/blah",
								Desc: "a svc",
								Kind: cloudapipb.AEK_SVC,
							},
						},
					},
					{
						Value:          "pl/frontend$0",
						Kind:           cloudapipb.AEK_SVC,
						Valid:          true,
						ContainsCursor: true,
					},
				},
				HasValidScript: true,
				Executable:     false,
			},
			action:      cloudapipb.AAT_SELECT,
			expectedStr: "${2:run} ${3:script:px/service_stats} ${4:svc:pl/$0} ${1:svc:pl/frontend}",
			expectedSuggestions: []*cloudapipb.TabSuggestion{
				{
					TabIndex:              2,
					ExecutableAfterSelect: false,
					Suggestions:           []*cloudapipb.AutocompleteSuggestion{},
				},
				{
					TabIndex:              3,
					ExecutableAfterSelect: false,
					Suggestions: []*cloudapipb.AutocompleteSuggestion{
						{
							Kind:        cloudapipb.AEK_SCRIPT,
							Name:        "px/service_stats",
							Description: "a svc",
						},
					},
				},
				{
					TabIndex:              4,
					ExecutableAfterSelect: true,
					Suggestions: []*cloudapipb.AutocompleteSuggestion{
						{
							Kind:        cloudapipb.AEK_SVC,
							Name:        "pl/blah",
							Description: "a svc",
						},
					},
				},
				{
					TabIndex:              1,
					ExecutableAfterSelect: false,
					Suggestions:           []*cloudapipb.AutocompleteSuggestion{},
				},
			},
		},
		{
			name: "add new tabstop",
			cmd: &autocomplete.Command{
				TabStops: []*autocomplete.TabStop{
					{
						Value: "run",
						Kind:  cloudapipb.AEK_UNKNOWN,
						Valid: true,
					},
					{
						Value:          "pl/",
						Kind:           cloudapipb.AEK_SVC,
						Valid:          false,
						ContainsCursor: false,
						Suggestions: []*autocomplete.Suggestion{
							{
								Name: "pl/blah",
								Desc: "a svc",
								Kind: cloudapipb.AEK_SVC,
							},
						},
					},
					{
						Value:          "pl/frontend$0",
						Kind:           cloudapipb.AEK_SVC,
						Valid:          true,
						ContainsCursor: true,
					},
				},
				Executable: false,
			},
			suggestionScriptTypes: []cloudapipb.AutocompleteEntityKind{
				cloudapipb.AEK_SVC,
			},
			callSuggester: true,
			action:        cloudapipb.AAT_SELECT,
			expectedStr:   "${1:run} ${2:svc:pl/} ${3:svc:pl/frontend} ${4:$0}",
			expectedSuggestions: []*cloudapipb.TabSuggestion{
				{
					TabIndex:              1,
					ExecutableAfterSelect: false,
					Suggestions:           []*cloudapipb.AutocompleteSuggestion{},
				},
				{
					TabIndex:              2,
					ExecutableAfterSelect: false,
					Suggestions: []*cloudapipb.AutocompleteSuggestion{
						{
							Kind:        cloudapipb.AEK_SVC,
							Name:        "pl/blah",
							Description: "a svc",
						},
					},
				},
				{
					TabIndex:              3,
					ExecutableAfterSelect: false,
					Suggestions:           []*cloudapipb.AutocompleteSuggestion{},
				},
				{
					TabIndex:              4,
					ExecutableAfterSelect: false,
					Suggestions: []*cloudapipb.AutocompleteSuggestion{
						{
							Kind:        cloudapipb.AEK_POD,
							Name:        "pl/test",
							Description: "default pod",
						},
					},
				},
			},
		},
		{
			name: "all valid",
			cmd: &autocomplete.Command{
				TabStops: []*autocomplete.TabStop{
					{
						Value: "run",
						Kind:  cloudapipb.AEK_UNKNOWN,
						Valid: true,
					},
					{
						Value:          "px/svc_info$0",
						Kind:           cloudapipb.AEK_SCRIPT,
						Valid:          true,
						ContainsCursor: true,
					},
					{
						Value:   "pl/frontend",
						Kind:    cloudapipb.AEK_SVC,
						Valid:   true,
						ArgName: "svc_name",
					},
				},
				Executable: false,
			},
			action:      cloudapipb.AAT_SELECT,
			expectedStr: "${1:run} ${2:script:px/svc_info} ${3:svc_name:pl/frontend$0}",
			expectedSuggestions: []*cloudapipb.TabSuggestion{
				{
					TabIndex:              1,
					ExecutableAfterSelect: false,
					Suggestions:           []*cloudapipb.AutocompleteSuggestion{},
				},
				{
					TabIndex:              2,
					ExecutableAfterSelect: false,
					Suggestions:           []*cloudapipb.AutocompleteSuggestion{},
				},
				{
					TabIndex:              3,
					ExecutableAfterSelect: false,
					Suggestions:           []*cloudapipb.AutocompleteSuggestion{},
				},
			},
		},
		{
			name: "invalid args",
			cmd: &autocomplete.Command{
				TabStops: []*autocomplete.TabStop{
					{
						Value: "run",
						Kind:  cloudapipb.AEK_UNKNOWN,
						Valid: true,
					},
					{
						Value: "px/svc_info",
						Kind:  cloudapipb.AEK_SCRIPT,
						Valid: false,
						Suggestions: []*autocomplete.Suggestion{
							{
								Name: "pl/svc_info_abc",
								Desc: "a svc",
								Kind: cloudapipb.AEK_SCRIPT,
							},
						},
					},
					{
						Value:          "pl/frontend$0",
						Kind:           cloudapipb.AEK_POD,
						Valid:          false,
						ContainsCursor: true,
						Suggestions: []*autocomplete.Suggestion{
							{
								Name:           "pl/frontend-test",
								Desc:           "a pod",
								Kind:           cloudapipb.AEK_POD,
								MatchedIndexes: []int64{1, 2, 3},
							},
						},
					},
				},
				Executable: false,
			},
			action:      cloudapipb.AAT_EDIT,
			expectedStr: "${1:run} ${2:script:px/svc_info} ${3:pod:pl/frontend$0}",
			expectedSuggestions: []*cloudapipb.TabSuggestion{
				{
					TabIndex:              1,
					ExecutableAfterSelect: false,
					Suggestions:           []*cloudapipb.AutocompleteSuggestion{},
				},
				{
					TabIndex:              2,
					ExecutableAfterSelect: false,
					Suggestions: []*cloudapipb.AutocompleteSuggestion{
						{
							Kind:        cloudapipb.AEK_SCRIPT,
							Name:        "pl/svc_info_abc",
							Description: "a svc",
						},
					},
				},
				{
					TabIndex:              3,
					ExecutableAfterSelect: false,
					Suggestions: []*cloudapipb.AutocompleteSuggestion{
						{
							Kind:           cloudapipb.AEK_POD,
							Name:           "pl/frontend-test",
							Description:    "a pod",
							MatchedIndexes: []int64{1, 2, 3},
						},
					},
				},
			},
		},
	}
	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			ctrl := gomock.NewController(t)
			defer ctrl.Finish()
			s := mock_autocomplete.NewMockSuggester(ctrl)

			orgID := uuid.Must(uuid.NewV4())

			if test.callSuggester {
				s.EXPECT().
					GetSuggestions([]*autocomplete.SuggestionRequest{
						{
							orgID, "test", "",
							[]cloudapipb.AutocompleteEntityKind{cloudapipb.AEK_POD, cloudapipb.AEK_SVC, cloudapipb.AEK_NAMESPACE, cloudapipb.AEK_SCRIPT},
							test.suggestionScriptTypes,
						},
					}).Return([]*autocomplete.SuggestionResult{
					{
						Suggestions: []*autocomplete.Suggestion{
							{
								Name: "pl/test",
								Kind: cloudapipb.AEK_POD,
								Desc: "default pod",
							},
						},
					},
				}, nil)
			}

			output, suggestions := test.cmd.ToFormatString(test.action, s, orgID, "test")
			assert.Equal(t, test.expectedStr, output)
			assert.ElementsMatch(t, test.expectedSuggestions, suggestions)
		})
	}
}
