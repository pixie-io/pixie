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

package esutils

import (
	"context"
	"encoding/json"
	"fmt"

	"github.com/olivere/elastic/v7"
)

// ManagedIndex creates a lifecycle managed index and handles migrating this index on startup.
// A ManagedIndex is collection of an elastic index (Index), along with an index template (IndexTemplate), and
// an elastic index lifecycle management policy (ILMPolicy).
//
// In elastic, indices can be in 1 of 4 phases (hot, warm, cold, delete). The ILMPolicy specifies actions for each
// phase, and transition conditions for going between the phases. Currently, the main use of the ManagedIndex is
// to specify transistioning from the hot phase to the delete phase after the index has reached a certain disk size.
// This is achieved using the "rollover" action  on the hot phase, which transistions an index into the next phase
// after the conditions have been met.
//
// Example usage:
//
//	err := NewManagedIndex(es, "my_managed_index").
//		IndexFromJSONString(`
//			{
//				"mappings": {
//					"properties": {
//						"my_text_field": {
//							"type": "text"
//						}
//					}
//				}
//			}`).
//		MaxIndexSize("1gb").
//		TimeBeforeDelete("1d").
//		Migrate(ctx)
//
// This example will create a new elastic index called "my_managed_index-000000". This index will have an index alias
// called "my_managed_index". It will also create an ILMPolicy that specifies that the index should be rolled over
// once it reaches 1gb in size. When that happens elastic will create a new index "my_managed_index-000001". Then 1 day
// after this rollover occurs, "my_managed_index-000000" will be deleted. Under the hood, ManagedIndex will also
// create an IndexTemplate. This is required by elastic so that when the rollover happens the new index
// "my_managed_index-000001", will still be associated to the ILMPolicy.
//
// Note that since an index alias is created called "my_managed_index", the consumer of this index doesn't need to
// care about the actual index name, and can just use the managed index's name in all future elastic calls as if it were
// the index name.
type ManagedIndex struct {
	es       *elastic.Client
	name     string
	policy   *ILMPolicy
	template *IndexTemplate
	index    *Index
}

// NewManagedIndex creates a new ManagedIndex with name.
func NewManagedIndex(es *elastic.Client, name string) *ManagedIndex {
	policyName := fmt.Sprintf("%s_policy", name)
	templateName := fmt.Sprintf("%s_template", name)
	return &ManagedIndex{
		es:       es,
		name:     name,
		policy:   NewILMPolicy(es, policyName),
		template: NewIndexTemplate(es, templateName).AssociateRolloverPolicy(policyName, name),
		index:    NewIndex(es),
	}
}

// MaxIndexSize sets the size to allow the index to grow to before rollover.
// the size should be specified as a string with unit, eg. "50gb".
func (m *ManagedIndex) MaxIndexSize(maxSize string) *ManagedIndex {
	m.policy.Rollover(&maxSize, nil, nil)
	return m
}

// MaxIndexAge sets how long the index will live until its rolled over.
// Rolled over indices can still be accessed until the Delete condition is reached.
// It should be specified as a string, eg. "30d".
func (m *ManagedIndex) MaxIndexAge(maxAge string) *ManagedIndex {
	m.policy.Rollover(nil, nil, &maxAge)
	return m
}

// TimeBeforeDelete sets the amount of time that should be waited after rollover, before deleting the index.
// The time should be specified as a string with unit, eg. "1d".
func (m *ManagedIndex) TimeBeforeDelete(timeBeforeDelete string) *ManagedIndex {
	m.policy.DeleteAfter(timeBeforeDelete)
	return m
}

// IndexFromJSONString sets the index config to be the json string provided.
// Example usage:
//
//	err := NewManagedIndex(es, "my_managed_index").IndexFromJSONString(
//	`
//	{
//			"mappings": <INSERT_MAPPINGS_HERE>,
//			"setings": <INSERT_SETTINGS_HERE>,
//	}
//	`
//	).Migrate(ctx)
//
// Note that as opposed to ILMPolicy(), creating the index
// config from json is likely to be done for every ManagedIndex so its a top level API.
// This will also add the mappings from the JSON string to the IndexTemplate.
func (m *ManagedIndex) IndexFromJSONString(j string) *ManagedIndex {
	m.index.FromJSONString(j)
	var indexJSON map[string]interface{}
	// index.FromJSONString checks if the json string can be unmarshalled so we can ignore the error here.
	_ = json.Unmarshal([]byte(j), &indexJSON)
	if _, ok := indexJSON["mappings"]; ok {
		m.template.AddIndexMappings(indexJSON["mappings"].(map[string]interface{}))
	}
	if _, ok := indexJSON["settings"]; ok {
		m.template.AddIndexSettings(indexJSON["settings"].(map[string]interface{}))
	}
	return m
}

// IndexTemplate gets the index template used by the ManagedIndex.
// This API is provided as a means of getting more fined-grained control
// over the index template used by the ManagedIndex.
// This API shouldn't be necessary for most simple use cases.
// Example usage:
//
//	myManagedInd := NewManagedIndex(es, "my_managed_ind")
//	myManagedInd.IndexTemplate().FromJSONString(`{<INSERT_JSON_HERE>}`)
//	err := myManagedInd.Migrate(ctx)
func (m *ManagedIndex) IndexTemplate() *IndexTemplate {
	return m.template
}

// ILMPolicy gets the ILMPolicy used by the ManagedIndex.
// This API is provided as a means of getting more fined-grained control
// over the ilm policy used by the ManagedIndex.
// Most simple use cases should be able to use the TimeBeforeDelete or MaxIndexSize APIs.
// Example usage:
//
//	myManagedInd := NewManagedIndex(es, "my_managed_ind")
//	myManagedInd.ILMPolicy().FromJSONString(`{<INSERT_JSON_HERE>}`)
//	err := myManagedInd.Migrate(ctx)
func (m *ManagedIndex) ILMPolicy() *ILMPolicy {
	return m.policy
}

// Migrate attempts to migrate from the current managed index to the new specification.
// See ilm_policy.go, index_template.go, and index.go for information about migrations
// of each of the corresponding objects.
func (m *ManagedIndex) Migrate(ctx context.Context) error {
	if err := m.policy.Migrate(ctx); err != nil {
		return err
	}
	if err := m.template.Migrate(ctx); err != nil {
		return err
	}

	m.setAliases()
	if err := m.setIndexName(ctx); err != nil {
		return err
	}
	if err := m.index.Migrate(ctx); err != nil {
		return err
	}
	return nil
}

func (m *ManagedIndex) setIndexName(ctx context.Context) error {
	aliasesResult, err := m.es.Aliases().Alias(m.name).Do(ctx)
	if err != nil {
		esErr, ok := err.(*elastic.Error)
		if !ok {
			return err
		}
		// Unforunately, olivere/elastic has no aliases exists endpoint so we have to check
		// the status code ourselves.
		if esErr.Status == 404 {
			// Alias doesn't exist, set indexName to be the alias followed by a -000000
			m.index.Name(fmt.Sprintf("%s-000000", m.name))
			return nil
		}
		return err
	}
	// Use the index that is the current write index for the alias. Elastic requires that there is
	// only one write index, so there ~SHOULD~ be only one matching index.
	for indName, indexResult := range aliasesResult.Indices {
		for _, aliasResult := range indexResult.Aliases {
			if aliasResult.AliasName != m.name {
				continue
			}
			if aliasResult.IsWriteIndex {
				m.index.Name(indName)
				return nil
			}
		}
	}
	// None of the indices were write indices, something went wrong.
	return fmt.Errorf("Failed to find an index that is a write index for the alias %s", m.name)
}

func (m *ManagedIndex) setAliases() {
	m.index.AddWriteAlias(m.name)
}
