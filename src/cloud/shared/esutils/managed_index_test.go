package esutils_test

import (
	"context"
	"fmt"
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
				require.NoError(t, err)
			}

			err := esutils.NewManagedIndex(elasticClient, tc.managedIndName).Migrate(context.Background())
			if tc.expectErr {
				require.NotNil(t, err)
				assert.Equal(t, tc.errMsg, err.Error())
				return
			}
			require.NoError(t, err)
			// Only cleanup if the creation was successful.
			defer cleanupManagedIndex(t, tc.managedIndName)

			respMap, err := elasticClient.IndexGet(tc.expectedIndexName).Do(context.Background())
			if err != nil {
				if err.(*elastic.Error).Status == 404 {
					t.Logf("Expected index '%s' was not found.", tc.expectedIndexName)
					t.FailNow()
				}
				require.NoError(t, err)
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
