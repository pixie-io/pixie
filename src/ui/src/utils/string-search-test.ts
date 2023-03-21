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

import { highlightNamespacedScoredMatch, highlightScoredMatch } from './string-search';

describe('String search utils', () => {
  it('Highlights perfect matches of normalized strings', () => {
    expect(highlightScoredMatch('Foo', 'fOO')).toEqual({ isMatch: true, distance: 0, highlights: [0, 1, 2] });
    expect(highlightScoredMatch('F_oo', '_fOO')).toEqual({ isMatch: true, distance: 0, highlights: [1, 2, 3] });
  });

  it('Highlights simple substring matches of normalized strings', () => {
    // Substring matches are treated as cheaper than typo matches (since it's all contiguos inserts), thus lower scores.
    expect(highlightScoredMatch('pod', 'px/pod'))
      .toEqual({ isMatch: true, distance: 0.75, highlights: [3, 4, 5] });
    expect(highlightScoredMatch('oba', 'foobar'))
      .toEqual({ isMatch: true, distance: 0.75, highlights: [2, 3, 4] });
    expect(highlightScoredMatch('o!Ba', '  F.o*O!bar__ '))
      .toEqual({ isMatch: true, distance: 0.75, highlights: [6, 8, 9] });
    expect(highlightScoredMatch('ace', 'namespace'))
      .toEqual({ isMatch: true, distance: 1.5, highlights: [6, 7, 8] });
    expect(highlightScoredMatch('ace', 'namespaces'))
      .toEqual({ isMatch: true, distance: 1.75, highlights: [6, 7, 8] });
  });

  it('Does not highlight strings that are too different to be reasonable matches', () => {
    // When the search has nothing in common with the source (zero characters match)
    expect(highlightScoredMatch('foo', 'bar'))
      .toEqual({ isMatch: false, distance: Infinity, highlights: [] });
    // When the highlights are too small of a percentage of the string being searched
    expect(highlightScoredMatch('a', Array(100).fill('a').join('')))
      .toEqual({ isMatch: false, distance: Infinity, highlights: [] });
    // When the edit distance is too high relative to the length of the strings
    expect(highlightScoredMatch('abcd', 'ade'))
      .toEqual({ isMatch: false, distance: Infinity, highlights: [] });
    expect(highlightScoredMatch('a  --Bc@D', 'ade'))
      .toEqual({ isMatch: false, distance: Infinity, highlights: [] });
  });

  it('Highlights matches that require Optimal String Alignment to detect', () => {
    // Note: We're using a slightly customized version of Optimal String Alignment.
    // Possible edits are:
    // - Insert a missing character
    // - Delete a character that isn't in the target string
    // - Substitute a character that's wrong, but is in the right position
    // - Transpose a character with its left neighbor (ba -> ab)
    // - Transpose a character with its left neighbor's left neighbor (cba -> abc)
    // - Nothing (character is already the same in this position in both strings)
    // The edits do not all cost the same (normally they'd all cost 1 "edit"); this is so that we can prioritize
    // edits that the user would believe look closer than edits they would not. That lets highlights look better too.
    // Thus, `distance` is no longer the actual edit distance, but rather a sort of pathfinding heuristic.

    // One insert and one delete (the x is moved by inserting a new one where it goes and deleting the old one)
    expect(highlightScoredMatch('p/pxod', 'px/pod'))
      .toEqual({ isMatch: true, distance: 2, highlights: [0, 2, 3, 4, 5] });
    expect(highlightScoredMatch('P*/ pxo?D', 'px/pod'))
      .toEqual({ isMatch: true, distance: 2, highlights: [0, 2, 3, 4, 5] });

    // One close swap (immediately adjacent characters)
    expect(highlightScoredMatch('ofo', 'foo'))
      .toEqual({ isMatch: true, distance: .5, highlights: [0, 1, 2] });
    expect(highlightScoredMatch('! o.fo%% =', 'foo'))
      .toEqual({ isMatch: true, distance: .5, highlights: [0, 1, 2] });

    // Three insertions, one far swap (two characters that are separated by a third).
    expect(highlightScoredMatch('dop', 'px/pod'))
      .toEqual({ isMatch: true, distance: 3.7, highlights: [3, 4, 5] });
    expect(highlightScoredMatch('D_ o&&p', 'px/pod'))
      .toEqual({ isMatch: true, distance: 3.7, highlights: [3, 4, 5] });

    // Two transpositions. In this case, every character in the source contributed to the match.
    expect(highlightScoredMatch('p/xpdo', 'px/pod'))
      .toEqual({ isMatch: true, distance: 1, highlights: [0, 1, 2, 3, 4, 5] });
    expect(highlightScoredMatch('P__/ xP*D?!o', 'px/pod'))
      .toEqual({ isMatch: true, distance: 1, highlights: [0, 1, 2, 3, 4, 5] });
  });

  it('Splits namespaced searches', () => {
    expect(highlightNamespacedScoredMatch('pod', 'px/pod', '/'))
      .toEqual({ isMatch: true, distance: 0.5, highlights: [3, 4, 5] });
    expect(highlightNamespacedScoredMatch('px/ace', 'px/namespace', '/'))
      .toEqual({ isMatch: true, distance: 1.5, highlights: [0, 1, 9, 10, 11] });
    expect(highlightNamespacedScoredMatch('px/ace', 'px/namespaces', '/'))
      .toEqual({ isMatch: true, distance: 1.75, highlights: [0, 1, 9, 10, 11] });
  });

  it('Scoring of multiple types of matches results in a reasonable ordering', () => {
    // Each of these searches should end up with the following order of lowest distance to highest.
    // The balancing on how to score different scenarios is delicate, but should result in something that makes the
    // most sense to a user. We're aiming for relevance, not similarity: find what the user _meant_.
    const searches = {
      // Perfect matches are much stronger than anything else
      'pod': ['px/POD', 'px/PODs', 'Px/nODe', 'nonsense'],
      // Changing a character (d to s) is a slightly stronger match than deleting the extra d
      'px/podd': ['PX/PODs', 'PX/POD'],
      // An unbroken substring is a stronger match than one with edits, even if the latter is a much shorter string.
      'px/ace': ['PX/namespACE', 'PX/namespACEs', 'PX/nodE'],
      'px/stat': ['PX/cql_STATs', 'PX/agent_STATus', 'PX/service_edge_STATs', 'PX/dnS_dATa', 'PXbeTA/service_endpoint'],
    };

    for (const search in searches) {
      const sources = searches[search];
      const results = sources.map(source => highlightNamespacedScoredMatch(search, source, '/'));
      const sorted = [...results].sort((a, b) => a.distance - b.distance);
      expect(results).toEqual(sorted);
    }
  });
});
