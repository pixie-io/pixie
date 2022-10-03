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

	"github.com/gofrs/uuid"
	"github.com/olivere/elastic/v7"
	"github.com/sahilm/fuzzy"

	"px.dev/pixie/src/api/proto/cloudpb"
	"px.dev/pixie/src/api/proto/vispb"
	"px.dev/pixie/src/cloud/indexer/md"
	"px.dev/pixie/src/cloud/profile/profilepb"
	"px.dev/pixie/src/utils/script"
)

// ElasticSuggester provides suggestions based on the given index in Elastic.
type ElasticSuggester struct {
	client          *elastic.Client
	mdIndexName     string
	scriptIndexName string
	pc              profilepb.ProfileServiceClient
	// This is temporary, and will be removed once we start indexing scripts.
	br *script.BundleManager
}

const (
	// Search for 1 more result than we return so we can efficiently
	// mark whether or not more results exist (without needing to find every
	// single result).
	resultLimit = 5
	searchLimit = resultLimit + 1
)

var protoToElasticLabelMap = map[cloudpb.AutocompleteEntityKind]md.EsMDType{
	cloudpb.AEK_SVC:       md.EsMDTypeService,
	cloudpb.AEK_POD:       md.EsMDTypePod,
	cloudpb.AEK_SCRIPT:    md.EsMDTypeScript,
	cloudpb.AEK_NAMESPACE: md.EsMDTypeNamespace,
	cloudpb.AEK_NODE:      md.EsMDTypeNode,
}

var elasticLabelToProtoMap = map[md.EsMDType]cloudpb.AutocompleteEntityKind{
	md.EsMDTypeService:   cloudpb.AEK_SVC,
	md.EsMDTypePod:       cloudpb.AEK_POD,
	md.EsMDTypeScript:    cloudpb.AEK_SCRIPT,
	md.EsMDTypeNamespace: cloudpb.AEK_NAMESPACE,
	md.EsMDTypeNode:      cloudpb.AEK_NODE,
}

var elasticStateToProtoMap = map[md.ESMDEntityState]cloudpb.AutocompleteEntityState{
	md.ESMDEntityStateUnknown:    cloudpb.AES_UNKNOWN,
	md.ESMDEntityStatePending:    cloudpb.AES_PENDING,
	md.ESMDEntityStateRunning:    cloudpb.AES_RUNNING,
	md.ESMDEntityStateFailed:     cloudpb.AES_FAILED,
	md.ESMDEntityStateTerminated: cloudpb.AES_TERMINATED,
}

// NewElasticSuggester creates a suggester based on an elastic index.
func NewElasticSuggester(client *elastic.Client, mdIndex, scriptIndex string, pc profilepb.ProfileServiceClient) (*ElasticSuggester, error) {
	return &ElasticSuggester{
		client:          client,
		mdIndexName:     mdIndex,
		scriptIndexName: scriptIndex,
		pc:              pc,
	}, nil
}

// SuggestionRequest is a request for autocomplete suggestions.
type SuggestionRequest struct {
	OrgID        uuid.UUID
	ClusterUID   string
	Input        string
	AllowedKinds []cloudpb.AutocompleteEntityKind
	AllowedArgs  []cloudpb.AutocompleteEntityKind
}

// SuggestionResult contains results for an autocomplete request.
type SuggestionResult struct {
	Suggestions          []*Suggestion
	ExactMatch           bool
	HasAdditionalMatches bool
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
func (e *ElasticSuggester) UpdateScriptBundle(br *script.BundleManager) {
	e.br = br
}

// GetSuggestions get suggestions for the given input using Elastic.
// It returns the suggestions, whether or not more matches exist, and an error.
func (e *ElasticSuggester) GetSuggestions(reqs []*SuggestionRequest) ([]*SuggestionResult, error) {
	br := e.br

	resps := make([]*SuggestionResult, len(reqs))

	if len(reqs) == 0 {
		return resps, nil
	}

	ms := e.client.MultiSearch()

	highlight := elastic.NewHighlight()
	highlight = highlight.Fields(elastic.NewHighlighterField("*"))

	// Deduplicate results by name, and prefer the result with the most recent updateVersion.
	coll := elastic.NewCollapseBuilder("name.keyword").InnerHit(elastic.NewInnerHit().Size(1).Name("collapse").Sort("updateVersion", false))

	for _, r := range reqs {
		ms.Add(elastic.NewSearchRequest().
			Highlight(highlight).
			Query(e.getQueryForRequest(r.OrgID, r.ClusterUID, r.Input, r.AllowedKinds, r.AllowedArgs)).FetchSourceIncludeExclude([]string{"kind", "name", "ns", "state", "updateVersion"}, []string{}).Collapse(coll).Size(searchLimit))
	}

	resp, err := ms.Do(context.Background())

	if err != nil {
		return nil, err
	}

	// Parse scripts to prepare for matching. This is temporary until we have script indexing.
	scripts := []string{}
	scriptArgMap := make(map[string][]cloudpb.AutocompleteEntityKind)
	scriptArgNames := make(map[string][]string)
	if br != nil {
		for _, s := range br.GetScripts() {
			scripts = append(scripts, s.ScriptName)
			scriptArgMap[s.ScriptName] = make([]cloudpb.AutocompleteEntityKind, 0)
			for _, a := range s.Vis.Variables {
				aKind := cloudpb.AEK_UNKNOWN
				if a.Type == vispb.PX_POD {
					aKind = cloudpb.AEK_POD
				} else if a.Type == vispb.PX_SERVICE {
					aKind = cloudpb.AEK_SVC
				}

				if aKind != cloudpb.AEK_UNKNOWN {
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
				if t == cloudpb.AEK_SCRIPT { // Script is an allowed type for this tabstop, so we should find matching scripts.
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
						if script.OrgID != reqs[i].OrgID.String() {
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
								Kind:           cloudpb.AEK_SCRIPT,
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
		hasAdditionalMatches := false
		results := make([]*Suggestion, 0)
		for _, h := range r.Hits.Hits {
			src := h.Source
			// Use the top-ranked result from the collapse. For some reason, this doesn't automatically
			// become the main hit in Elastic and we need to pull it out of the innerHits.
			// The innerHits from the collapse should always be defined, so this just is extra defensive.
			if h.InnerHits["collapse"].Hits != nil && len(h.InnerHits["collapse"].Hits.Hits) > 0 {
				src = h.InnerHits["collapse"].Hits.Hits[0].Source
			}
			res := &md.EsMDEntity{}
			err = json.Unmarshal(src, res)
			if err != nil {
				return nil, err
			}

			matchedIndexes := make([]int64, 0)
			// Parse highlight string into indexes.
			if len(h.Highlight["name"]) > 0 {
				matchedIndexes = append(matchedIndexes, parseHighlightIndexes(h.Highlight["name"][0], 0)...)
			}

			// TODO(michellenguyen): Remove namespace handling when we create a new index and ensure there are no more
			// documents with namespace.
			resName := res.Name
			if res.NS != "" && !(md.EsMDType(res.Kind) == md.EsMDTypeNamespace || md.EsMDType(res.Kind) == md.EsMDTypeNode) {
				resName = fmt.Sprintf("%s/%s", res.NS, res.Name)
			}

			// We asked for resultLimit+1 results but will send only resultLimit.
			// This way, we can communicate to the downstream consumer whether or not more
			// results are present for that search term.
			if len(results) == resultLimit {
				hasAdditionalMatches = true
				break
			}
			results = append(results, &Suggestion{
				Name:           resName,
				Score:          float64(*h.Score),
				Kind:           elasticLabelToProtoMap[md.EsMDType(res.Kind)],
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
			Suggestions:          results,
			ExactMatch:           exactMatch,
			HasAdditionalMatches: hasAdditionalMatches,
		}
	}

	return resps, nil
}

func (e *ElasticSuggester) getQueryForRequest(orgID uuid.UUID, clusterUID string, input string, allowedKinds []cloudpb.AutocompleteEntityKind, allowedArgs []cloudpb.AutocompleteEntityKind) *elastic.BoolQuery {
	q := elastic.NewBoolQuery()

	q.Should(e.getMDEntityQuery(orgID, clusterUID, input, allowedKinds))

	// Once script indexing is in, we should also query the scripts: q.Should(e.getScriptQuery(orgID, input, allowedArgs))
	return q
}

func (e *ElasticSuggester) getMDEntityQuery(orgID uuid.UUID, clusterUID string, input string, allowedKinds []cloudpb.AutocompleteEntityKind) *elastic.BoolQuery {
	entityQuery := elastic.NewBoolQuery()
	entityQuery.Must(elastic.NewTermQuery("_index", e.mdIndexName))

	// Use a boosting query to rank running resources higher than terminated resources.
	negativeQuery := elastic.NewBoolQuery()
	negativeQuery.Should(elastic.NewTermQuery("state", md.ESMDEntityStateFailed)).Should(elastic.NewTermQuery("state", md.ESMDEntityStateTerminated)).Should(elastic.NewTermQuery("state", md.ESMDEntityStateUnknown))
	positiveQuery := elastic.NewBoolQuery()
	positiveQuery.Should(elastic.NewTermQuery("state", md.ESMDEntityStateRunning)).Should(elastic.NewTermQuery("state", md.ESMDEntityStatePending))
	entityQuery.Should(elastic.NewBoostingQuery().Positive(positiveQuery).Negative(negativeQuery).NegativeBoost(0.7))

	// If the user hasn't provided any input string, don't run bother running a match query.
	if len(input) >= 1 {
		entityQuery.Must(elastic.NewMatchQuery("name", input))
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
