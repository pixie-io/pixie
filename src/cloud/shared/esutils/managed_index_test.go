package esutils_test

import (
	"context"
	"encoding/json"
	"fmt"
	"math/rand"
	"testing"

	"github.com/olivere/elastic/v7"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"pixielabs.ai/pixielabs/src/cloud/shared/esutils"
)

func cleanupManagedIndex(t *testing.T, managedIndexName string) {
	templateName := fmt.Sprintf("%s_template", managedIndexName)
	policyName := fmt.Sprintf("%s_policy", managedIndexName)
	indexWildcardName := fmt.Sprintf("%s-*", managedIndexName)
	cleanupIndex(t, indexWildcardName)
	cleanupTemplate(t, templateName)
	cleanupPolicy(t, policyName)
}

func TestManagedIndexMigrate(t *testing.T) {
	testCases := []struct {
		name               string
		managedIndName     string
		indexJSON          string
		expectErr          bool
		errMsg             string
		createBeforeConfig *struct {
			indexName string
		}
		expectedIndexName string
	}{
		{
			name:               "starts index number at 0 if no indices exist with the managedIndName as alias",
			managedIndName:     "test_managed_create",
			expectErr:          false,
			expectedIndexName:  "test_managed_create-000000",
			createBeforeConfig: nil,
		},
		{
			name:              "uses index name from alias if one exists",
			managedIndName:    "test_managed_find_existing_ind",
			expectErr:         false,
			expectedIndexName: "test_managed_find_exists_ind-000005",
			createBeforeConfig: &struct {
				indexName string
			}{
				indexName: "test_managed_find_exists_ind-000005",
			},
		},
	}

	for _, tc := range testCases {
		t.Run(tc.name, func(t *testing.T) {

			if tc.createBeforeConfig != nil {
				i := esutils.NewIndex(elasticClient).Name(tc.createBeforeConfig.indexName)
				i.AddWriteAlias(tc.managedIndName)
				err := i.Migrate(context.Background())
				require.Nil(t, err)
			}

			err := esutils.NewManagedIndex(elasticClient, tc.managedIndName).Migrate(context.Background())
			if tc.expectErr {
				require.NotNil(t, err)
				assert.Equal(t, tc.errMsg, err.Error())
				return
			}
			require.Nil(t, err)
			// Only cleanup if the creation was successful.
			defer cleanupManagedIndex(t, tc.managedIndName)

			respMap, err := elasticClient.IndexGet(tc.expectedIndexName).Do(context.Background())
			if err != nil {
				if err.(*elastic.Error).Status == 404 {
					t.Logf("Expected index '%s' was not found.", tc.expectedIndexName)
					t.FailNow()
				}
				require.Nil(t, err)
			}
			resp, ok := respMap[tc.expectedIndexName]
			require.True(t, ok)

			expectedAliases := map[string]interface{}{
				tc.managedIndName: map[string]interface{}{
					"is_write_index": true,
				},
			}

			assert.Equal(t, expectedAliases, resp.Aliases)

		})
	}
}

var letterRunes = []rune("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ")

func randStringRunes(n int) string {
	b := make([]rune, n)
	for i := range b {
		b[i] = letterRunes[rand.Intn(len(letterRunes))]
	}
	return string(b)
}

func assertHasAllExpected(t *testing.T, expectedJSON string, actualResp *elastic.IndicesGetResponse) {
	var ok bool
	var expectedResp map[string]interface{}
	err := json.Unmarshal([]byte(expectedJSON), &expectedResp)
	require.Nil(t, err)

	var expectedMappings map[string]interface{}
	if expectedResp["mappings"] != nil {
		expectedMappings, ok = expectedResp["mappings"].(map[string]interface{})
		require.True(t, ok)
	} else {
		expectedMappings = make(map[string]interface{})
	}
	var expectedAliases map[string]interface{}
	if expectedResp["aliases"] != nil {
		expectedAliases, ok = expectedResp["aliases"].(map[string]interface{})
		require.True(t, ok)
	} else {
		expectedAliases = make(map[string]interface{})
	}
	var expectedSettings map[string]interface{}
	if expectedResp["settings"] != nil {
		expectedSettings, ok = expectedResp["settings"].(map[string]interface{})
		require.True(t, ok)
	} else {
		expectedSettings = make(map[string]interface{})
	}
	assertHasAllExpectedMap(t, expectedMappings, actualResp.Mappings)
	assertHasAllExpectedMap(t, expectedAliases, actualResp.Aliases)
	assertHasAllExpectedMap(t, expectedSettings, actualResp.Settings)
}

func assertHasAllExpectedMap(t *testing.T, expected map[string]interface{}, actual map[string]interface{}) {
	for k, expectedV := range expected {
		actualV, ok := actual[k]
		require.True(t, ok)
		if expectedM, ok := expectedV.(map[string]interface{}); ok {
			actualM, ok := actualV.(map[string]interface{})
			require.True(t, ok)
			assertHasAllExpectedMap(t, expectedM, actualM)
		} else {
			assert.Equalf(t, fmt.Sprintf("%v", expectedV), fmt.Sprintf("%v", actualV), "not equal for key %s", k)
		}
	}
}
