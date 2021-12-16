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

const normalRegExp = new RegExp('[^a-z0-9/]+', 'gi');

/** Turns "  px/be_spoke-  " into "px/bespoke". That is: lowercase, remove anything that isn't alphanum or a slash. */
export function normalize(s: string): string {
  return s.toLowerCase().replace(normalRegExp, '');
}

/**
 * Highlights the first occurrence, character by character, of the search string in the source string.
 * Works as if 'search' became the regular expression /^.*s.*e.*a.*r.*c.*h.*$/.
 * Ignores characters that would be removed by {@link normalize}, and ignores case.
 *
 * For example: searching for 'foobar' in 'baz/foo_bar' would highlight like: 'baz/FOO_BAR'.
 * Searching for 'foobar' in 'faz/foobar' would highlight 'Faz/fOOBAR'.
 * An incomplete match highlights nothing.
 *
 * Returns an array of positions in the source string that should be highlighted.
 * Returns an empty array if the match was incomplete (for instance, 'scorch' would not match 'search').
 */
export function highlightMatch(search: string, source: string): number[] {
  const highlights = [];

  let sourceOffset = 0;
  let searchOffset = 0;
  for (; searchOffset < search.length && sourceOffset < source.length; searchOffset++) {
    const c = normalize(search[searchOffset]);
    if (!c.length) continue; // Skip characters we don't match against
    while (sourceOffset < source.length) {
      const s = normalize(source[sourceOffset]);
      if (s.length && s === c) {
        highlights.push(sourceOffset);
        sourceOffset++;
        break;
      }
      sourceOffset++;
    }
  }

  // If the match was not complete, treat it as no match.
  if (highlights.length < normalize(search).length) {
    return [];
  }

  return highlights;
}
