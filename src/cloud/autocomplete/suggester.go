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

package autocomplete

import (
	"context"
	"encoding/json"
	"fmt"
	"strings"
	"sync"

	"github.com/gofrs/uuid"
	"github.com/olivere/elastic/v7"
	"github.com/sahilm/fuzzy"
	"github.com/spf13/viper"
	"google.golang.org/grpc/metadata"

	pl_vispb "px.dev/pixie/src/api/proto/vispb"
	"px.dev/pixie/src/cloud/cloudapipb"
	"px.dev/pixie/src/cloud/indexer/md"
	profilepb "px.dev/pixie/src/cloud/profile/profilepb"
	"px.dev/pixie/src/pixie_cli/pkg/script"
	srvutils "px.dev/pixie/src/shared/services/utils"
	"px.dev/pixie/src/utils"
)

// ElasticSuggester provides suggestions based on the given index in Elastic.
type ElasticSuggester struct {
	client          *elastic.Client
	scriptIndexName string
	pc              profilepb.ProfileServiceClient
	// This is temporary, and will be removed once we start indexing scripts.
	br         *script.BundleManager
	orgMapping map[uuid.UUID]string
	bundleMu   sync.Mutex
}

var protoToElasticLabelMap = map[cloudapipb.AutocompleteEntityKind]string{
	cloudapipb.AEK_SVC:       "service",
	cloudapipb.AEK_POD:       "pod",
	cloudapipb.AEK_SCRIPT:    "script",
	cloudapipb.AEK_NAMESPACE: "script",
}

var elasticLabelToProtoMap = map[string]cloudapipb.AutocompleteEntityKind{
	"service":   cloudapipb.AEK_SVC,
	"pod":       cloudapipb.AEK_POD,
	"script":    cloudapipb.AEK_SCRIPT,
	"namespace": cloudapipb.AEK_NAMESPACE,
}

var elasticStateToProtoMap = map[md.ESMDEntityState]cloudapipb.AutocompleteEntityState{
	md.ESMDEntityStateUnknown:    cloudapipb.AES_UNKNOWN,
	md.ESMDEntityStatePending:    cloudapipb.AES_PENDING,
	md.ESMDEntityStateRunning:    cloudapipb.AES_RUNNING,
	md.ESMDEntityStateFailed:     cloudapipb.AES_FAILED,
	md.ESMDEntityStateTerminated: cloudapipb.AES_TERMINATED,
}

func getServiceCredentials(signingKey string) (string, error) {
	claims := srvutils.GenerateJWTForService("API Service", viper.GetString("domain_name"))
	return srvutils.SignJWTClaims(claims, signingKey)
}

// NewElasticSuggester creates a suggester based on an elastic index.
func NewElasticSuggester(client *elastic.Client, scriptIndex string, pc profilepb.ProfileServiceClient) (*ElasticSuggester, error) {
	return &ElasticSuggester{
		client:          client,
		scriptIndexName: scriptIndex,
		pc:              pc,
		orgMapping:      make(map[uuid.UUID]string),
	}, nil
}

// SuggestionRequest is a request for autocomplete suggestions.
type SuggestionRequest struct {
	OrgID        uuid.UUID
	ClusterUID   string
	Input        string
	AllowedKinds []cloudapipb.AutocompleteEntityKind
	AllowedArgs  []cloudapipb.AutocompleteEntityKind
}

// SuggestionResult contains results for an autocomplete request.
type SuggestionResult struct {
	Suggestions []*Suggestion
	ExactMatch  bool
}

func parseHighlightIndexes(highlightStr string, offset int) []int64 {
	inHighlight := false
	j := int64(offset)
	k := 0
	matchedIndexes := make([]int64, 0)
	for k < len(highlightStr) {
		if string(highlightStr[k]) == "<" {
			if inHighlight { // Ending highlight.
				inHighlight = false
				k += len("</em>")
			} else { // Starting highlight section.
				inHighlight = true
				k += len("<em>")
			}
			continue
		}
		if inHighlight {
			matchedIndexes = append(matchedIndexes, j)
		}
		k++
		j++
	}
	return matchedIndexes
}

// UpdateScriptBundle updates the script bundle used to populate the suggester's script suggestions.
func (e *ElasticSuggester) UpdateScriptBundle(br *script.BundleManager) error {
	orgMapping := make(map[uuid.UUID]string)
	if br != nil {
		// Generate mapping from orgID -> orgName. This is temporary until we have a script indexer.
		serviceAuthToken, err := getServiceCredentials(viper.GetString("jwt_signing_key"))
		if err != nil {
			return err
		}
		ctx := metadata.AppendToOutgoingContext(context.Background(), "authorization",
			fmt.Sprintf("bearer %s", serviceAuthToken))
		resp, err := e.pc.GetOrgs(ctx, &profilepb.GetOrgsRequest{})
		if err != nil {
			return err
		}

		for _, org := range resp.Orgs {
			orgMapping[utils.UUIDFromProtoOrNil(org.ID)] = org.OrgName
		}
	}

	e.bundleMu.Lock()
	defer e.bundleMu.Unlock()
	e.orgMapping = orgMapping
	e.br = br

	return nil
}

// GetSuggestions get suggestions for the given input using Elastic.
func (e *ElasticSuggester) GetSuggestions(reqs []*SuggestionRequest) ([]*SuggestionResult, error) {
	e.bundleMu.Lock()
	br := e.br
	orgMapping := e.orgMapping
	e.bundleMu.Unlock()

	resps := make([]*SuggestionResult, len(reqs))

	if len(reqs) == 0 {
		return resps, nil
	}

	ms := e.client.MultiSearch()

	highlight := elastic.NewHighlight()
	highlight = highlight.Fields(elastic.NewHighlighterField("*"))

	for _, r := range reqs {
		ms.Add(elastic.NewSearchRequest().
			Highlight(highlight).
			Query(e.getQueryForRequest(r.OrgID, r.ClusterUID, r.Input, r.AllowedKinds, r.AllowedArgs)).
			Size(5).FetchSourceIncludeExclude([]string{"kind", "name", "ns", "state"}, []string{}))
	}

	resp, err := ms.Do(context.Background())

	if err != nil {
		return nil, err
	}

	// Parse scripts to prepare for matching. This is temporary until we have script indexing.
	scripts := []string{}
	scriptArgMap := make(map[string][]cloudapipb.AutocompleteEntityKind)
	scriptArgNames := make(map[string][]string)
	if br != nil {
		for _, s := range br.GetScripts() {
			scripts = append(scripts, s.ScriptName)
			scriptArgMap[s.ScriptName] = make([]cloudapipb.AutocompleteEntityKind, 0)
			for _, a := range s.Vis.Variables {
				aKind := cloudapipb.AEK_UNKNOWN
				if a.Type == pl_vispb.PX_POD {
					aKind = cloudapipb.AEK_POD
				} else if a.Type == pl_vispb.PX_SERVICE {
					aKind = cloudapipb.AEK_SVC
				}

				if aKind != cloudapipb.AEK_UNKNOWN {
					scriptArgMap[s.ScriptName] = append(scriptArgMap[s.ScriptName], aKind)
					scriptArgNames[s.ScriptName] = append(scriptArgNames[s.ScriptName], a.Name)
				}
			}
		}
	}

	for i, r := range resp.Responses {
		// This is temporary until we index scripts in Elastic.
		scriptResults := make([]*Suggestion, 0)
		if br != nil {
			for _, t := range reqs[i].AllowedKinds {
				if t == cloudapipb.AEK_SCRIPT { // Script is an allowed type for this tabstop, so we should find matching scripts.
					matches := fuzzy.Find(reqs[i].Input, scripts)

					if reqs[i].Input == "" { // The input is empty, so none of the scripts will match using the fuzzy search.
						matches = make([]fuzzy.Match, len(scripts))
						for i, s := range scripts {
							matches[i] = fuzzy.Match{
								Str:            s,
								MatchedIndexes: make([]int, 0),
							}
						}
					}
					for _, m := range matches {
						script := br.MustGetScript(m.Str)
						scriptArgs := scriptArgMap[m.Str]
						scriptNames := scriptArgNames[m.Str]
						valid := true
						if script.OrgName != "" && orgMapping[reqs[i].OrgID] != script.OrgName {
							valid = false
						}

						for _, r := range reqs[i].AllowedArgs { // Check that the script takes the allowed args.
							found := false
							for _, arg := range scriptArgs {
								if arg == r {
									found = true
									break
								}
							}
							if !found {
								valid = false
								break
							}
						}
						if valid {
							matchedIdxs := make([]int64, len(m.MatchedIndexes))
							for i, matched := range m.MatchedIndexes {
								matchedIdxs[i] = int64(matched)
							}
							scriptResults = append(scriptResults, &Suggestion{
								Name:           m.Str,
								Kind:           cloudapipb.AEK_SCRIPT,
								Desc:           script.LongDoc,
								ArgNames:       scriptNames,
								ArgKinds:       scriptArgs,
								MatchedIndexes: matchedIdxs,
							})
						}
					}
					break
				}
			}
		}
		exactMatch := len(scriptResults) > 0 && scriptResults[0].Name == reqs[i].Input

		// Convert elastic entity into a suggestion object.
		results := make([]*Suggestion, 0)
		for _, h := range r.Hits.Hits {
			res := &md.EsMDEntity{}
			err = json.Unmarshal(h.Source, res)
			if err != nil {
				return nil, err
			}

			matchedIndexes := make([]int64, 0)
			// Parse highlight string into indexes.
			if len(h.Highlight["ns"]) > 0 {
				matchedIndexes = append(matchedIndexes, parseHighlightIndexes(h.Highlight["ns"][0], 0)...)
			}
			if len(h.Highlight["name"]) > 0 {
				matchedIndexes = append(matchedIndexes, parseHighlightIndexes(h.Highlight["name"][0], len(res.NS)+1)...)
			}
			results = append(results, &Suggestion{
				Name:           res.NS + "/" + res.Name,
				Score:          float64(*h.Score),
				Kind:           elasticLabelToProtoMap[res.Kind],
				MatchedIndexes: matchedIndexes,
				State:          elasticStateToProtoMap[res.State],
			})
		}

		for _, r := range results {
			// If any of the suggested results exactly matches the typed-in argument, then
			// we consider the argument an exact match, and therefore a valid argument.
			// Usually the exact input is always the first argument, but elastic ranking
			// can be weird sometimes.
			exactMatch = exactMatch || r.Name == reqs[i].Input
		}

		results = append(scriptResults, results...)

		resps[i] = &SuggestionResult{
			Suggestions: results,
			ExactMatch:  exactMatch,
		}
	}
	return resps, nil
}

func (e *ElasticSuggester) getQueryForRequest(orgID uuid.UUID, clusterUID string, input string, allowedKinds []cloudapipb.AutocompleteEntityKind, allowedArgs []cloudapipb.AutocompleteEntityKind) *elastic.BoolQuery {
	q := elastic.NewBoolQuery()

	q.Should(e.getMDEntityQuery(orgID, clusterUID, input, allowedKinds))

	// Once script indexing is in, we should also query the scripts: q.Should(e.getScriptQuery(orgID, input, allowedArgs))
	return q
}

func (e *ElasticSuggester) getMDEntityQuery(orgID uuid.UUID, clusterUID string, input string, allowedKinds []cloudapipb.AutocompleteEntityKind) *elastic.BoolQuery {
	entityQuery := elastic.NewBoolQuery()
	entityQuery.Must(elastic.NewTermQuery("_index", md.IndexName))

	// Search by name + namespace.
	splitInput := strings.Split(input, "/") // If contains "/", then everything preceding "/" is a namespace.
	name := input
	if len(splitInput) > 1 {
		entityQuery.Must(elastic.NewMatchQuery("ns", splitInput[0]))
		name = splitInput[1]

		if name != "" {
			entityQuery.Must(elastic.NewMatchQuery("name", name))
		}
	} else if name != "" {
		nsOrNameQuery := elastic.NewMultiMatchQuery(name, "name", "ns")
		entityQuery.Must(nsOrNameQuery)
	}

	// Only search for entities in org.
	entityQuery.Must(elastic.NewTermQuery("orgID", orgID.String()))

	if clusterUID != "" {
		entityQuery.Must(elastic.NewTermQuery("clusterUID", clusterUID))
	}

	// Only search for allowed kinds.
	kindsQuery := elastic.NewBoolQuery()
	for _, k := range allowedKinds {
		kindsQuery.Should(elastic.NewTermQuery("kind", protoToElasticLabelMap[k]))
	}
	entityQuery.Must(kindsQuery)

	return entityQuery
}
