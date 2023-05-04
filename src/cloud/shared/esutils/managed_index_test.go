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

package esutils_test

import (
	"context"
	"fmt"
	"testing"

	"github.com/olivere/elastic/v7"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"px.dev/pixie/src/cloud/shared/esutils"
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

			err := esutils.NewManagedIndex(elasticClient, tc.managedIndName).MaxIndexSize("1gb").TimeBeforeDelete("10d").Migrate(context.Background())
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
