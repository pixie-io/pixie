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

type esIndexTemplate struct {
	IndexPatterns []string               `json:"index_patterns"`
	Settings      map[string]interface{} `json:"settings,omitempty"`
	Mappings      map[string]interface{} `json:"mappings,omitempty"`
	Aliases       map[string]interface{} `json:"aliases,omitempty"`
}

// IndexTemplate  manages the creation/update of elasticsearch index templates.
type IndexTemplate struct {
	es                  *elastic.Client
	templateName        string
	template            *esIndexTemplate
	errorDuringAssembly error
}

// NewIndexTemplate creates a new IndexTemplate with the given name.
func NewIndexTemplate(es *elastic.Client, templateName string) *IndexTemplate {
	return &IndexTemplate{
		es:           es,
		templateName: templateName,
		template: &esIndexTemplate{
			IndexPatterns: []string{},
			Settings:      make(map[string]interface{}),
			Aliases:       make(map[string]interface{}),
			Mappings:      make(map[string]interface{}),
		},
	}
}

// AssociateRolloverPolicy associates an ILMPolicy with this index template.
// This ensures that all indices that match the index pattern will be under the pervue of the given policy.
func (t *IndexTemplate) AssociateRolloverPolicy(policyName, aliasName string) *IndexTemplate {
	t.template.IndexPatterns = append(t.template.IndexPatterns, fmt.Sprintf("%s-*", aliasName))
	if t.template.Settings["index"] == nil {
		t.template.Settings["index"] = make(map[string]interface{})
	}
	indexMap := t.template.Settings["index"].(map[string]interface{})
	if indexMap["lifecycle"] == nil {
		indexMap["lifecycle"] = make(map[string]interface{})
	}
	lifecycleMap := indexMap["lifecycle"].(map[string]interface{})
	lifecycleMap["name"] = policyName
	lifecycleMap["rollover_alias"] = aliasName
	return t
}

// Migrate creates and/or updates the elastic index template to conform to the spec in t.template.
func (t *IndexTemplate) Migrate(ctx context.Context) error {
	if err := t.validate(); err != nil {
		return err
	}
	return t.upsert(ctx)
}

// FromJSONString populates the index template spec from a marshalled json string.
// This is useful if more fine-grained control of the index template is required.
func (t *IndexTemplate) FromJSONString(templJSONStr string) *IndexTemplate {
	if err := json.Unmarshal([]byte(templJSONStr), &t.template); err != nil {
		t.errorDuringAssembly = err
	}
	return t
}

// AddIndexMappings populates the mappings of the index template from the map passed.
// Although, elastic supposedly copies the mappings over when there is an index rollover,
// there seem to be some cases where this doesn't happen,
// so we add the index mappings to the index template to make sure it happens.
func (t *IndexTemplate) AddIndexMappings(mappings map[string]interface{}) *IndexTemplate {
	t.template.Mappings = mappings
	return t
}

// AddIndexSettings populates the settings of the index template from the map passed in.
// I thought that elastic copied over the settings on index rollover, but it seems that doesn't happen sometimes,
// so copying the settings to the template to make sure that happens.
func (t *IndexTemplate) AddIndexSettings(settings map[string]interface{}) *IndexTemplate {
	for k, v := range settings {
		if m, ok := v.(map[string]interface{}); ok {
			if t.template.Settings[k] == nil {
				t.template.Settings[k] = make(map[string]interface{})
			}
			for childK, childV := range m {
				t.template.Settings[k].(map[string]interface{})[childK] = childV
			}
		} else {
			t.template.Settings[k] = v
		}
	}
	return t
}

func (t *IndexTemplate) validate() error {
	if len(t.template.IndexPatterns) == 0 {
		return fmt.Errorf("must specify at least 1 index pattern to create an index template")
	}
	if t.errorDuringAssembly != nil {
		return t.errorDuringAssembly
	}
	return nil
}

func (t *IndexTemplate) upsert(ctx context.Context) error {
	jsonBody, err := json.Marshal(t.template)
	if err != nil {
		return err
	}
	resp, err := t.es.IndexPutTemplate(t.templateName).
		BodyString(string(jsonBody)).
		Do(ctx)
	if err != nil {
		return err
	}
	if !resp.Acknowledged {
		return fmt.Errorf("failed to create index template '%s'", t.templateName)
	}
	return nil
}
