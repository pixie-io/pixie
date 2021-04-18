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
	"encoding/json"
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"px.dev/pixie/src/cloud/shared/esutils"
)

func cleanupTemplate(t *testing.T, templateName string) {
	resp, err := elasticClient.IndexDeleteTemplate(templateName).Do(context.Background())
	require.NoError(t, err)
	require.True(t, resp.Acknowledged)
}

func TestIndexTemplateMigrate(t *testing.T) {
	testCases := []struct {
		name               string
		policyName         string
		templateName       string
		aliasName          string
		expectErr          bool
		createBeforeConfig *struct {
			policyName string
			aliasName  string
		}
	}{
		{
			name:               "creates a new template when one doesn't exist",
			policyName:         "test_index_templ_policy",
			templateName:       "test_index_template",
			aliasName:          "test_index",
			expectErr:          false,
			createBeforeConfig: nil,
		},
		{
			name:         "updates a template if the policyName is different",
			policyName:   "test_index_templ_update_policy",
			templateName: "test_index_templ_update",
			aliasName:    "test_index_update",
			expectErr:    false,
			createBeforeConfig: &struct {
				policyName string
				aliasName  string
			}{
				policyName: "test_index_templ_update_policy_old",
				aliasName:  "test_index_update",
			},
		},
		{
			name:         "updates a template if the aliasName is different",
			policyName:   "test_index_templ_update_policy",
			templateName: "test_index_templ_update",
			aliasName:    "test_index_update",
			expectErr:    false,
			createBeforeConfig: &struct {
				policyName string
				aliasName  string
			}{
				policyName: "test_index_templ_update_policy",
				aliasName:  "test_index_update_old",
			},
		},
	}

	for _, tc := range testCases {
		t.Run(tc.name, func(t *testing.T) {
			if tc.createBeforeConfig != nil {
				err := esutils.NewIndexTemplate(elasticClient, tc.templateName).
					AssociateRolloverPolicy(tc.createBeforeConfig.policyName, tc.createBeforeConfig.aliasName).
					Migrate(context.Background())
				require.NoError(t, err)
			}

			expectedTemplate := esutils.NewIndexTemplate(elasticClient, tc.templateName).
				AssociateRolloverPolicy(tc.policyName, tc.aliasName)
			err := expectedTemplate.Migrate(context.Background())
			if tc.expectErr {
				require.NotNil(t, err)
				return
			}
			require.NoError(t, err)
			// Only cleanup if the creation was successful.
			defer cleanupTemplate(t, tc.templateName)

			respMap, err := elasticClient.IndexGetTemplate(tc.templateName).Do(context.Background())
			require.NoError(t, err)
			resp, ok := respMap[tc.templateName]
			require.True(t, ok)

			respStr, err := json.Marshal(resp)
			require.NoError(t, err)

			actualTemplate := esutils.NewIndexTemplate(elasticClient, tc.templateName).FromJSONString(string(respStr))

			assert.Equal(t, expectedTemplate, actualTemplate)
		})
	}
}
