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

package md

import (
	"context"
	"fmt"

	"github.com/olivere/elastic/v7"

	"px.dev/pixie/src/cloud/shared/esutils"
)

// ESMDEntityState represents state for a metadata entity in elastic.
type ESMDEntityState int

const (
	// ESMDEntityStateUnknown is when the state of the entity is unknown.
	ESMDEntityStateUnknown ESMDEntityState = iota
	// ESMDEntityStatePending represents the pending state.
	ESMDEntityStatePending
	// ESMDEntityStateRunning represents the running state.
	ESMDEntityStateRunning
	// ESMDEntityStateFailed represents the failed state.
	ESMDEntityStateFailed
	// ESMDEntityStateTerminated represents the terminated state.
	ESMDEntityStateTerminated
)

// EsMDType are the types for metadata entities in elastic.
type EsMDType string

const (
	// EsMDTypeNamespace is for namespace entities.
	EsMDTypeNamespace EsMDType = "namespace"
	// EsMDTypeService is for service entities.
	EsMDTypeService EsMDType = "service"
	// EsMDTypePod is for pod entities.
	EsMDTypePod EsMDType = "pod"
	// EsMDTypeScript is for script entities.
	EsMDTypeScript EsMDType = "script"
	// EsMDTypeNode is for node entities.
	EsMDTypeNode EsMDType = "node"
)

// EsMDEntity is the struct that is stored in elastic.
type EsMDEntity struct {
	OrgID      string `json:"orgID"`
	ClusterUID string `json:"clusterUID"`
	VizierID   string `json:"vizierID"`
	UID        string `json:"uid"`
	Name       string `json:"name"`
	NS         string `json:"ns"`
	Kind       string `json:"kind"`

	TimeStartedNS int64 `json:"timeStartedNS"`
	TimeStoppedNS int64 `json:"timeStoppedNS"`

	RelatedEntityNames []string `json:"relatedEntityNames"`

	UpdateVersion int64 `json:"updateVersion"`

	State ESMDEntityState `json:"state"`
}

// IndexMapping is the index structure for metadata entities.
// TODO(michellenguyen): Remove namespace from the index once we stop writing and reading from it.
const IndexMapping = `
{
  "settings": {
    "index": {
      "store": {
        "preload": [
          "nvd",
          "dvd",
          "tim",
          "doc",
          "dim"
        ]
      },
      "max_ngram_diff": "10",
      "number_of_shards": "4",
      "analysis": {
        "filter": {
          "dont_split_on_numerics": {
            "type": "word_delimiter",
            "preserve_original": "true",
            "generate_number_parts": "false"
          }
        },
        "tokenizer": {
          "ngram_tokenizer": {
            "type": "nGram",
            "min_gram": "1",
            "max_gram": "10",
            "token_chars": [
              "letter",
              "digit",
              "custom"
            ],
            "custom_token_chars": "-/"
          },
          "search_tokenizer": {
            "type": "char_group",
            "tokenize_on_chars": [
              "whitespace",
	            "-",
              "\n",
	            "/",
	            "_"
            ]
          }
        },
        "analyzer": {
          "autocomplete": {
            "type": "custom",
            "tokenizer": "ngram_tokenizer",
            "filter": [
              "lowercase"
            ]
          },
          "autocomplete_search": {
            "tokenizer": "search_tokenizer",
            "filter": [
              "lowercase"
            ]
          },
          "myAnalyzer": {
            "type": "custom",
            "tokenizer": "whitespace",
            "filter": [
              "dont_split_on_numerics"
            ]
          }
        }
      }
    }
  },
  "mappings": {
    "properties": {
      "orgID": {
        "type": "text",
        "analyzer": "myAnalyzer",
        "eager_global_ordinals": true
      },
      "vizierID": {
        "type": "text",
        "analyzer": "myAnalyzer"
      },
      "clusterUID": {
        "type": "text",
        "analyzer": "myAnalyzer",
        "eager_global_ordinals": true
      },
      "uid": {
        "type": "text"
      },
      "name": {
        "type": "text",
        "analyzer": "autocomplete",
        "search_analyzer": "autocomplete_search",
        "eager_global_ordinals": true,
        "fields": {
          "keyword": {
            "type": "keyword"
          }
        }
      },
      "kind": {
        "type": "text",
        "eager_global_ordinals": true
      },
      "timeStartedNS": {
        "type": "long"
      },
      "timeStoppedNS": {
        "type": "long"
      },
      "relatedEntityNames": {
        "type": "text"
      },
      "updateVersion": {
        "type": "long"
      },
      "state": {
        "type": "integer"
      }
    }
  }
}
`

// InitializeMapping creates the index in elastic.
func InitializeMapping(es *elastic.Client, indexName string, replicas int, maxAge string, deleteAfter string, manualIndex bool) error {
	ctx := context.Background()
	if manualIndex {
		exists, err := es.IndexExists(indexName).Do(ctx)
		if err != nil {
			return err
		}
		if !exists {
			return fmt.Errorf("elastic index %s does not exist, but manual index management specified", indexName)
		}
		// Update the index mappings if necessary.
		err = esutils.NewIndex(es).Name(indexName).FromJSONString(IndexMapping).Migrate(ctx)
		if err != nil {
			return err
		}
	} else {
		err := esutils.NewManagedIndex(es, indexName).
			IndexFromJSONString(IndexMapping).
			MaxIndexAge(maxAge).
			TimeBeforeDelete(deleteAfter).
			Migrate(ctx)
		if err != nil {
			return err
		}
	}

	replicaSetting := fmt.Sprintf("{\"index\": {\"number_of_replicas\": %d}}", replicas)
	_, err := es.IndexPutSettings(indexName).BodyString(replicaSetting).Do(context.Background())
	return err
}
