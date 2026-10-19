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

// NoopSuggester is a suggester that returns empty results.
// It is used when autocomplete is disabled (e.g., when Elasticsearch is not available).
type NoopSuggester struct{}

// NewNoopSuggester creates a new NoopSuggester.
func NewNoopSuggester() *NoopSuggester {
	return &NoopSuggester{}
}

// GetSuggestions returns empty results for all requests.
func (n *NoopSuggester) GetSuggestions(reqs []*SuggestionRequest) ([]*SuggestionResult, error) {
	results := make([]*SuggestionResult, len(reqs))
	for i := range reqs {
		results[i] = &SuggestionResult{
			Suggestions:          []*Suggestion{},
			ExactMatch:           false,
			HasAdditionalMatches: false,
		}
	}
	return results, nil
}
