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
	"errors"
	"fmt"
	"reflect"
	"strings"

	"github.com/olivere/elastic/v7"
	log "github.com/sirupsen/logrus"
)

// esIndex is a struct representation of the index json returned by Elasticsearch.
type esIndex struct {
	Mappings map[string]interface{} `json:"mappings,omitempty"`
	Aliases  map[string]interface{} `json:"aliases,omitempty"`
	Settings map[string]interface{} `json:"settings,omitempty"`
}

// Index handles the creation and/or updating of elasticsearch indices.
type Index struct {
	es                  *elastic.Client
	indexName           string
	forceUpdate         bool
	errorDuringAssembly error
	index               *esIndex
}

// NewIndex creates a new Index.
func NewIndex(es *elastic.Client) *Index {
	return &Index{
		es:          es,
		forceUpdate: false,
		index: &esIndex{
			Mappings: make(map[string]interface{}),
			Settings: make(map[string]interface{}),
			Aliases:  make(map[string]interface{}),
		},
	}
}

// Name sets the name of the Index.
func (i *Index) Name(indexName string) *Index {
	i.indexName = indexName
	return i
}

// FromJSONString populates the index spec from a marshalled json string.
func (i *Index) FromJSONString(jsonStr string) *Index {
	if err := json.Unmarshal([]byte(jsonStr), &i.index); err != nil {
		i.errorDuringAssembly = err
	}
	return i
}

// AddWriteAlias adds an index alias for this index, with is_write_index set to true.
// Note that only one index per alias can have is_write_index set to true.
func (i *Index) AddWriteAlias(name string) *Index {
	if i.index.Aliases[name] == nil {
		i.index.Aliases[name] = make(map[string]interface{})
	}
	alias, ok := i.index.Aliases[name].(map[string]interface{})
	if !ok {
		i.errorDuringAssembly = fmt.Errorf("each alias must be of type map[string]interface{}")
	}
	alias["is_write_index"] = true
	return i
}

// Migrate creates and/or updates the elasticsearch index to conform to the spec in i.index.
// Currently, Migrate does not support reindexing of indices to conform to the new spec.
// This means certain changes to mappings or settings can't be updated through this migration.
// But many types of changes to index mappings/settings/aliases are still handled by Migrate.
func (i *Index) Migrate(ctx context.Context) error {
	if err := i.validate(); err != nil {
		return err
	}
	exists, err := i.exists(ctx)
	if err != nil {
		return err
	}
	if !exists {
		return i.create(ctx)
	}
	return i.update(ctx)
}

func (i *Index) validate() error {
	if i.indexName == "" {
		return fmt.Errorf("must call `Index.Name(name)` to set the name of the index to be created")
	}
	if i.errorDuringAssembly != nil {
		return fmt.Errorf("error assembling desired index config: %s", i.errorDuringAssembly.Error())
	}
	return nil
}

func (i *Index) exists(ctx context.Context) (bool, error) {
	return i.es.IndexExists(i.indexName).Do(ctx)
}

func (i *Index) create(ctx context.Context) error {
	log.WithField("index", i.indexName).Trace("Creating index")
	indexBody, err := json.Marshal(i.index)
	if err != nil {
		return err
	}
	resp, err := i.es.CreateIndex(i.indexName).BodyString(string(indexBody)).Do(ctx)
	if err != nil {
		var cause map[string]interface{}
		if esError, ok := err.(*elastic.Error); ok {
			cause = esError.Details.CausedBy
		}
		log.WithError(err).WithField("index", i.indexName).
			WithField("body", string(indexBody)).
			WithField("cause", cause).
			Error("failed to create index")
		return err
	}
	if !resp.Acknowledged {
		return fmt.Errorf("failed to create index '%s'", i.indexName)
	}
	return nil
}

func (i *Index) update(ctx context.Context) error {
	log.WithField("index", i.indexName).Trace("Updating index")
	reindexReqMappings, err := i.updateMappings(ctx)
	if err != nil {
		return err
	}
	reindexReqSettings, err := i.updateSettings(ctx)
	if err != nil {
		return err
	}
	if err := i.updateAliases(ctx); err != nil {
		return err
	}

	if reindexReqMappings || reindexReqSettings {
		return i.reindex(ctx)
	}
	return nil
}

func (i *Index) updateMappings(ctx context.Context) (bool, error) {
	if len(i.index.Mappings) == 0 {
		return false, nil
	}
	resp, err := i.es.PutMapping().Index(i.indexName).BodyJson(i.index.Mappings).Do(ctx)
	if err != nil {
		// If the error was a failure to merge the mapping with the current index
		// mapping, then a reindex is required.
		if mustParseError(err).isMappingMergeFailure() {
			log.WithError(err).Warn("cant merge mappings")
			return true, nil
		}
		log.WithError(err).WithField("index", i.indexName).WithField("cause", err.(*elastic.Error).Details.CausedBy).
			Error("failed to update index mappings")
		return false, err
	}
	if !resp.Acknowledged {
		return false, fmt.Errorf("failed to update mappings for index '%s'", i.indexName)
	}
	return false, nil
}

// updates returns the updates necessary to make current match desired.
// if desired is a map then the updates can be a subset of the map,
// for all other types the updates are either nil or the desired value.
func updates(desired interface{}, current interface{}) interface{} {
	if reflect.TypeOf(desired) != reflect.TypeOf(current) {
		return desired
	}
	switch desired.(type) {
	case map[string]interface{}:
		cm := current.(map[string]interface{})
		dm := desired.(map[string]interface{})
		toUpdate := make(map[string]interface{})
		for k, d := range dm {
			c, ok := cm[k]
			if !ok {
				toUpdate[k] = d
				continue
			}
			update := updates(d, c)
			if update != nil {
				toUpdate[k] = update
			}
		}
		if len(toUpdate) > 0 {
			return toUpdate
		}
		return nil
	case []interface{}:
		cl := current.([]interface{})
		dl := desired.([]interface{})
		// We consider lists to be non-mergable, so if there's any difference we say the whole list needs to change.
		if len(cl) != len(dl) {
			return dl
		}
		for i, d := range dl {
			if updates(d, cl[i]) != nil {
				return d
			}
		}
		return nil
	case string:
		cs := current.(string)
		ds := desired.(string)
		if cs != ds {
			return ds
		}
		return nil
	default:
		// The elastic settings response should always be maps, lists, or strings, if another case arises we rely on golangs comparison of interfaces.
		if current != desired {
			return desired
		}
		return nil
	}
}

func (i *Index) updateSettings(ctx context.Context) (bool, error) {
	settingsResp, err := i.es.IndexGetSettings(i.indexName).Do(ctx)
	if err != nil {
		log.WithError(err).WithField("index", i.indexName).
			WithField("cause", err.(*elastic.Error).Details.CausedBy).Error("failed to get index settings")
		return false, err
	}
	var indexResp *elastic.IndicesGetSettingsResponse
	for indexName, resp := range settingsResp {
		// If `i.indexName` is an alias, then the response can be the full index name instead of the alias name.
		if strings.HasPrefix(indexName, i.indexName) {
			indexResp = resp
			break
		}
	}
	if indexResp == nil {
		return false, errors.New("could not get index settings")
	}
	currentSettings := indexResp.Settings
	diff := updates(i.index.Settings, currentSettings)
	if diff == nil {
		return false, nil
	}
	settingsToUpdate := diff.(map[string]interface{})
	resp, err := i.es.IndexPutSettings(i.indexName).BodyJson(settingsToUpdate).Do(ctx)
	if err != nil {
		// If any of the non-dynamic settings changed, then a reindex is required.
		if mustParseError(err).isSettingsNonDynamicUpdateFailure() {
			log.WithError(err).WithField("old_settings", currentSettings).
				WithField("new_settings", i.index.Settings).
				WithField("update", settingsToUpdate).
				Warn("attempted to update non-dynamic settings")
			return true, nil
		}
		log.WithError(err).WithField("index", i.indexName).WithField("cause", err.(*elastic.Error).Details.CausedBy).
			Error("failed to update index settings")
		return false, err
	}
	if !resp.Acknowledged {
		return false, fmt.Errorf("failed to update settings for '%s'", i.indexName)
	}
	return false, nil
}

func (i *Index) updateAliases(ctx context.Context) error {
	currentAliases, err := i.es.Aliases().Index(i.indexName).Do(ctx)
	if err != nil {
		return err
	}
	currentIndexAliases := currentAliases.Indices[i.indexName].Aliases

	aliasActions := make([]elastic.AliasAction, 0)

	for aliasName, aliasSettings := range i.index.Aliases {
		addAction, err := i.createAliasAddAction(aliasName, aliasSettings)
		if err != nil {
			return err
		}
		aliasActions = append(aliasActions, addAction)
	}

	for _, alias := range currentIndexAliases {
		_, ok := i.index.Aliases[alias.AliasName]
		// Remove aliases that aren't in the desiredIndex.
		if !ok {
			aliasActions = append(aliasActions, i.createAliasRemoveAction(alias.AliasName))
		}
	}

	if len(aliasActions) == 0 {
		return nil
	}
	// Apply actions to elastic.
	resp, err := i.es.Alias().Action(aliasActions...).Do(ctx)
	if err != nil {
		log.WithError(err).WithField("index", i.indexName).WithField("cause", err.(*elastic.Error).Details.CausedBy).
			Error("failed to update index aliases")
		return err
	}
	if !resp.Acknowledged {
		return fmt.Errorf("failed to update index aliases")
	}
	return nil
}

func (i *Index) createAliasRemoveAction(aliasName string) elastic.AliasAction {
	return elastic.NewAliasRemoveAction(aliasName).Index(i.indexName)
}

func (i *Index) createAliasAddAction(aliasName string, aliasSettings interface{}) (elastic.AliasAction, error) {
	addAction := elastic.NewAliasAddAction(aliasName).Index(i.indexName)
	// It's unforunate that olivere/elastic has no way of updating aliases by JSON
	// so we have to parse the things we care about into olivere/elastic func calls.
	aliasSettingsMap, ok := aliasSettings.(map[string]interface{})
	if !ok {
		return nil, fmt.Errorf("each alias must be of type map[string]interface{}")
	}
	if isWriteIndex, ok := aliasSettingsMap["is_write_index"]; ok {
		isWriteIndexBool, ok := isWriteIndex.(bool)
		if !ok {
			return nil, fmt.Errorf("is_write_index must be of type bool")
		}
		addAction.IsWriteIndex(isWriteIndexBool)
	}
	return addAction, nil
}

func (i *Index) reindex(ctx context.Context) error {
	return fmt.Errorf("reindexing is currently not supported")
}
