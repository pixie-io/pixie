package esutils_test

import (
	"context"
	"fmt"
	"math/rand"
	"testing"
	"time"

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

// This test is really slow because I have to wait for elastic to do certain things.
func TestManagedIndexRollover(t *testing.T) {
	testCases := []struct {
		name                     string
		managedIndName           string
		maxIndexSize             string
		maxSizeBytes             int64
		timeBeforeDelete         string
		timeBeforeDeleteDuration time.Duration
	}{
		{
			name:                     "index rollsover after maxIndexSize reached",
			managedIndName:           "test_managed_rollover_max_index_size",
			maxIndexSize:             "50kb",
			maxSizeBytes:             50 * 1024,
			timeBeforeDelete:         "3s",
			timeBeforeDeleteDuration: 3 * time.Second,
		},
	}

	for _, tc := range testCases {
		t.Run(tc.name, func(t *testing.T) {
			indexJSON := `
			{
				"mappings": {
					"properties": {
						"test_field": {
							"type": "text"
						}
					}
				}
			}`
			err := esutils.NewManagedIndex(elasticClient, tc.managedIndName).
				IndexFromJSONString(indexJSON).
				MaxIndexSize(tc.maxIndexSize).
				TimeBeforeDelete(tc.timeBeforeDelete).
				Migrate(context.Background())
			if err != nil {
				t.Logf("ErrorDetails: %v", err.(*elastic.Error).Details)
			}
			require.Nil(t, err)
			defer cleanupManagedIndex(t, tc.managedIndName)

			currentSize := int64(0)
			for currentSize < tc.maxSizeBytes {
				fiveKbStr := randStringRunes(5 * 1024)
				docToIndex := fmt.Sprintf(`{"test_field": "%s"}`, fiveKbStr)
				_, err = elasticClient.Index().Index(tc.managedIndName).BodyString(docToIndex).Do(context.Background())
				require.Nil(t, err)
				stats, err := elasticClient.IndexStats(tc.managedIndName).Do(context.Background())
				require.Nil(t, err)
				currentSize = stats.All.Total.Store.SizeInBytes
			}

			// Wait for ILM poll_interval which is set to 5s in //src/utils/testingutils/elastic.go
			time.Sleep(5 * time.Second)
			start := time.Now()
			aliases, err := elasticClient.Aliases().Alias(tc.managedIndName).Do(context.Background())
			require.Nil(t, err)
			aliasCallDuration := time.Since(start)

			if aliasCallDuration < tc.timeBeforeDeleteDuration {
				_, ok := aliases.Indices[fmt.Sprintf("%s-000000", tc.managedIndName)]
				assert.Truef(t, ok, "Old index shouldn't be deleted before timeBeforeDelete")
			}
			alias, ok := aliases.Indices[fmt.Sprintf("%s-000001", tc.managedIndName)]
			assert.Truef(t, ok, "New rollover index should've been created")

			type aliasResult struct {
				AliasName    string
				IsWriteIndex bool
			}
			aliasResults := make([]aliasResult, len(alias.Aliases))
			for i, a := range alias.Aliases {
				aliasResults[i] = aliasResult{
					AliasName:    a.AliasName,
					IsWriteIndex: a.IsWriteIndex,
				}
			}
			assert.Contains(t, aliasResults, aliasResult{
				AliasName:    tc.managedIndName,
				IsWriteIndex: true,
			})

			time.Sleep(tc.timeBeforeDeleteDuration - aliasCallDuration)

			aliases, err = elasticClient.Aliases().Alias(tc.managedIndName).Do(context.Background())
			require.Nil(t, err)
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
