package controller_test

import (
	"testing"

	"github.com/golang/mock/gomock"
	"github.com/graph-gophers/graphql-go/gqltesting"

	"pixielabs.ai/pixielabs/src/cloud/api/controller/testutils"
	"pixielabs.ai/pixielabs/src/cloud/cloudapipb"
)

func TestAutocomplete(t *testing.T) {
	gqlEnv, _, _, _, as, cleanup := testutils.CreateTestGraphQLEnv(t)
	defer cleanup()
	ctx := CreateTestContext()

	as.EXPECT().Autocomplete(gomock.Any(), &cloudapipb.AutocompleteRequest{
		Input:     "px/svc_info svc:pl/test",
		CursorPos: 0,
		Action:    cloudapipb.AAT_EDIT,
	}).
		Return(&cloudapipb.AutocompleteResponse{
			FormattedInput: "${2:run} ${3:$0px/svc_info} ${1:svc:pl/test}",
			IsExecutable:   false,
			TabSuggestions: []*cloudapipb.TabSuggestion{
				&cloudapipb.TabSuggestion{
					TabIndex:              2,
					ExecutableAfterSelect: false,
					Suggestions:           []*cloudapipb.AutocompleteSuggestion{},
				},
				&cloudapipb.TabSuggestion{
					TabIndex:              3,
					ExecutableAfterSelect: false,
					Suggestions: []*cloudapipb.AutocompleteSuggestion{
						&cloudapipb.AutocompleteSuggestion{
							Kind:           cloudapipb.AEK_POD,
							Name:           "svc_info_pod",
							Description:    "this is a pod",
							MatchedIndexes: []int64{0, 1, 2},
						},
					},
				},
				&cloudapipb.TabSuggestion{
					TabIndex:              1,
					ExecutableAfterSelect: false,
					Suggestions: []*cloudapipb.AutocompleteSuggestion{
						&cloudapipb.AutocompleteSuggestion{
							Kind:           cloudapipb.AEK_SVC,
							Name:           "pl/test",
							Description:    "this is a svc",
							MatchedIndexes: []int64{5, 6, 7},
						},
					},
				},
			},
		}, nil)

	gqlSchema := LoadSchema(gqlEnv)
	gqltesting.RunTests(t, []*gqltesting.Test{
		{
			Schema:  gqlSchema,
			Context: ctx,
			Query: `
				query {
					autocomplete(input: "px/svc_info svc:pl/test", cursorPos: 0, action: AAT_EDIT) {
						formattedInput
						isExecutable 
						tabSuggestions {
							tabIndex 
							executableAfterSelect
							suggestions {
								kind 
								name 
								description
								matchedIndexes
							}
						}
					}
				}
			`,
			ExpectedResult: `
				{
					"autocomplete": {
						"formattedInput": "${2:run} ${3:$0px/svc_info} ${1:svc:pl/test}",
						"isExecutable": false,
						"tabSuggestions": [
							{ "tabIndex": 2, "executableAfterSelect": false, "suggestions": []},
							{ "tabIndex": 3, "executableAfterSelect": false, "suggestions": 
								[{"kind": "AEK_POD", "name": "svc_info_pod", "description": "this is a pod", "matchedIndexes": [0, 1, 2]}]
							},
							{ "tabIndex": 1, "executableAfterSelect": false, "suggestions": 
								[{"kind": "AEK_SVC", "name": "pl/test", "description": "this is a svc", "matchedIndexes": [5, 6, 7]}]
							}
						]
					}
				}
			`,
		},
	})
}
