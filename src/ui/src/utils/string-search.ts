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

const ALLOWED_CHARS = new Set('abcdefghijklmnopqrstuvwxyz01234567890/'.split(''));

// Scoring by relevance involves a bit of math based on what properties a match has.
// These numbers were determined experimentally, by trying a variety of search strings against
// the full list of PxL script names until the ordering consistently got desirable results.
const PREFIX_DISTANCE_FACTOR = 0.2;
const SUBSTRING_DISTANCE_FACTOR = 0.25;
const MAX_DISTANCE_RATIO = 0.8;
const INSERT_COST = 1;
const DELETE_COST = 1;
const SUBSTITUTE_COST = 0.75;
const NEAR_SWAP_COST = 0.50;
const FAR_SWAP_COST = 0.70;

interface ScoredStringMatch {
  /** Whether the search string is a reasonably confident match for the source string */
  isMatch: boolean;
  /** 0 means the search was an exact copy of the source. Higher numbers are weaker matches; Infinity mean no match. */
  distance: number;
  /** If there was a match, this is each index into the source string where an individual character matched. */
  highlights: number[];
}

/**
 * A faster (but much less compact) way of doing this:
 *   const out = str.toLowerCase().split('').map((c, i) => ({ c, i })).filter(({ c }) => ALLOWED_CHARS.has(c));
 *   const norm = out.map(({ c }) => c).join('');
 * Handling it the long way is noticeably faster when `highlightScoreMatch` gets called hundreds of times per render.
 *
 * @param str String to normalize
 * @returns That string, lowercased and with forbidden characters removed; plus a mapping back to the original string.
 */
function normForScore(str: string): { norm: string, chars: Array<{ i: number, c: string }> } {
  let j = 0;
  const out = Array(str.length).fill(0);
  let norm = '';
  for (let i = 0; i < str.length; i++) {
    const c = str.substring(i, i + 1).toLowerCase();
    if (ALLOWED_CHARS.has(c)) {
      out[j] = { i, c };
      norm += c;
      j++;
    }
  }
  out.length = j; // Faster to overallocate and cut off at the end than to keep growing the array.
  return { chars: out, norm };
}

// Some matches are just too far fetched to make sense, so filter those out
function isMatchGoodEnough(searchLen: number, sourceLen: number, distance: number, highlights: number[]): boolean {
  const min = Math.min(searchLen, sourceLen);
  // Must highlight at least half of the smaller string
  if (highlights.length <= Math.ceil(min / 2)) return false;
  // Edit distance should be within reason (too high, and we probably have a totally unrelated string)
  if (distance >= (Math.max(searchLen, sourceLen) * MAX_DISTANCE_RATIO)) return false;
  return true;
}

/**
 * The complex case for {@link highlightScoredMatch}.
 * Does Optimal String Alignment, with weighted costs for different transformations, and keeps track of the "path" it
 * takes so it can highlight the characters that contributed to the match.
 * See: https://en.wikipedia.org/wiki/Damerau%E2%80%93Levenshtein_distance#Optimal_string_alignment_distance
 *
 * This function is only called if {@link highlightScoredMatch} could not cheaply find a simpler match.
 * Both search and source are the results of pre-normalizing and tracking indices into the original strings.
 */
function optimalStringAlignmentAndHighlight(
  search: Array<{ i: number, c: string }>,
  source: Array<{ i: number, c: string }>,
): ScoredStringMatch {
  const highlights = new Set<number>();
  const d = Array(search.length + 1);
  for (let di = 0; di <= search.length; di++) {
    d[di] = Array(source.length + 1).fill(0);
    d[di][0] = di;
  }
  for (let dj = 0; dj <= source.length; dj++) d[0][dj] = dj;

  // We use this to build the highlights after finding the score. See below
  const prev = new Map<string, { k: number, l: number, op: 'del' | 'ins' | 'nop' | 'sub' | 'swp' | 'fswp' }>();

  for (let k = 1; k <= search.length; k++) {
    for (let l = 1; l <= source.length; l++) {
      const canSubstitute = search[k - 1].c !== source[l - 1].c;
      const canSwap = k > 1 && l > 1
        && search[k - 1].c === source[l - 2].c
        && search[k - 2].c === source[l - 1].c;
      const canSwapFar = k > 2 && l > 2
        && search[k - 1].c === source[l - 3].c
        && search[k - 3].c === source[l - 1].c;

      // Insert and delete "cost" more than substitution, which costs more than swapping.
      // This means we're not tracking "how many edits does it take"; we're preferring edits that look better to humans.
      const deleteDist = d[k - 1][l] + DELETE_COST;
      const insertDist = d[k][l - 1] + INSERT_COST;
      const substituteDist = canSubstitute ? d[k - 1][l - 1] + SUBSTITUTE_COST : Infinity;
      const noopDist = canSubstitute ? Infinity : d[k - 1][l - 1];
      const swapDist = canSwap ? d[k - 2][l - 2] + NEAR_SWAP_COST : Infinity;
      const farSwapDist = canSwapFar ? d[k - 3][l - 3] + FAR_SWAP_COST : Infinity;
      d[k][l] = Math.min(deleteDist, insertDist, substituteDist, noopDist, swapDist, farSwapDist);

      // Keep track of the path we took, so we can trace it back for highlights later
      const pk = `${k},${l}`;
      switch (d[k][l]) {
        case deleteDist: prev.set(pk, { k: k - 1, l: l, op: 'del' }); break;
        case insertDist: prev.set(pk, { k: k, l: l - 1, op: 'ins' }); break;
        case substituteDist: prev.set(pk, { k: k - 1, l: l - 1, op: 'sub' }); break;
        case noopDist: prev.set(pk, { k: k - 1, l: l - 1, op: 'nop' }); break;
        case swapDist: prev.set(pk, { k: k - 2, l: l - 2, op: 'swp' }); break;
        case farSwapDist: prev.set(pk, { k: k - 1, l: l - 1, op: 'fswp' }); break;
        default: throw new Error('Impossible: distance matrix made an illegal operation');
      }
    }
  }

  // Retrace the steps we took to find the optimal string alignment, to mark which characters in the source string were
  // actually kept from the search string (that means characters that were not inserts or deletes or substitutions).
  // Structurally, this ends up looking a lot like Dijkstra's algorithm for pathfinding.
  let ks = search.length;
  let ls = source.length;
  while (ks > 0 && ls > 0) {
    const { k, l, op } = prev.get(`${ks},${ls}`);
    if (op === 'swp') {
      // Add both characters that moved
      highlights.add(source[ls - 1].i);
      highlights.add(source[l].i);
    } else if (op === 'fswp') {
      // Same, but the distance was greater
      highlights.add(source[ls - 1].i);
      highlights.add(source[ls - 3].i);
    } else if (op === 'nop') {
      // No-op means that character existed in the source and the search in the same relative position.
      highlights.add(source[l].i);
    }
    ls = l;
    ks = k;
  }

  const distance = d[search.length][source.length];
  const h = [...highlights];
  h.sort((a, b) => a - b);
  if (isMatchGoodEnough(search.length, source.length, distance, h)) {
    return { isMatch: true, distance, highlights: h };
  }

  return { isMatch: false, distance: Infinity, highlights: [] };
}

/**
 * Performs a fuzzy string match, scores match strength, and highlights contributing characters in the source string.
 *
 * @param search What to look for
 * @param source Where to look for it
 * @returns Highlights indices in the source string that resembled the search, and returns the strength of the match.
 *          A strength of 0 means perfect match (source === search); Infinity means no match.
 */
export function highlightScoredMatch(search: string, source: string): ScoredStringMatch {
  const failCase = { isMatch: false, distance: Infinity, highlights: [] };

  // Before we begin, normalize both the search and the source.
  const { chars: searchChars, norm: searchNorm } = normForScore(search);
  const { chars: sourceChars, norm: sourceNorm } = normForScore(source);

  // Perfect match: distance is 0
  if (searchNorm === sourceNorm) {
    return {
      isMatch: true,
      distance: 0,
      highlights: sourceChars.map(({ i }) => i),
    };
  }

  // Simple substring match: distance is the length difference between the strings
  const exactSubstringIndex = sourceNorm.indexOf(searchNorm);
  if (exactSubstringIndex === 0 && searchNorm.length >= 3) {
    // Exact prefix is even better than exact substring, but not as good as a perfect match
    return {
      isMatch: true,
      distance: Math.abs(searchNorm.length - sourceNorm.length) * PREFIX_DISTANCE_FACTOR,
      highlights: Array(searchNorm.length).fill(0).map((_, i) => i),
    };
  } else if (exactSubstringIndex >= 0) {
    const start = sourceChars[exactSubstringIndex].i;
    // The edit distance for a substring is quite high, but substring matches are often better than typo matches.
    const distance = Math.abs(sourceChars.length - searchChars.length) * SUBSTRING_DISTANCE_FACTOR;
    const highlights = Array(searchChars.length);
    for (let j = 0; j < searchChars.length; j++) {
      highlights[j] = start + searchChars[j].i;
    }
    if (isMatchGoodEnough(
      search.length,
      source.length,
      Math.abs(sourceChars.length - searchChars.length),
      highlights,
    )) {
      return { isMatch: true, distance, highlights };
    } else {
      // Too far apart, return early
      return failCase;
    }
  }

  // Check if the strings have anything in common at all before we proceed with the expensive match...
  if (!sourceChars.some(({ c }) => searchNorm.includes(c))) {
    return failCase;
  }

  return optimalStringAlignmentAndHighlight(searchChars, sourceChars);
}

/**
 * Calls {@link highlightScoredMatch}, splitting the source string into namespaced parts to search them individually.
 * Then returns the best match it finds this way.
 *
 * For example, search=`pod`, source=`px/pod`, delimiter=`/` -> { isMatch: true, distance: 0, highlights: [3, 4, 5] }
 *
 * @param search String to look for
 * @param source String to look into, will be split on namespaceDelimiter
 * @param namespaceDelimiter What character separates parts of the source string
 * @returns
 */
export function highlightNamespacedScoredMatch(
  search: string,
  source: string,
  namespaceDelimiter: string,
): ScoredStringMatch {
  if (!source.includes(namespaceDelimiter)) {
    return highlightScoredMatch(search, source);
  }

  let splitCost = 0;
  const parts = source.split(namespaceDelimiter).filter(p => p);
  if (search.includes(namespaceDelimiter)) {
    const searchParts = search.split(namespaceDelimiter).filter(p => p);
    if (searchParts.length === parts.length) {
      const matches = searchParts.map((p, i) => highlightScoredMatch(p, parts[i]));
      if (matches.every(m => m.isMatch)) {
        let sumLen = 0;
        for (let i = 0; i < parts.length; i++) {
          const match = matches[i];
          for (let j = 0; j < match.highlights.length; j++) match.highlights[j] += sumLen;
          sumLen += 1 + parts[i].length;
        }
        return {
          isMatch: true,
          distance: matches.reduce((a, c) => a + c.distance, 0),
          highlights: matches.map(m => m.highlights).flat(1),
        };
      }
    }
    splitCost = 1; // We failed to find matches in every part, so consider the delimiter as part of the distance below.
  }

  const matches = [highlightScoredMatch(search, source)];
  let sumLen = 0;
  for (let i = 0; i < parts.length; i++) {
    const match = highlightScoredMatch(search, parts[i]);
    for (let j = 0; j < match.highlights.length; j++) match.highlights[j] += sumLen;
    sumLen += 1 + parts[i].length;
    match.distance += splitCost + parts.filter((_, k) => k !== i).reduce(
      (a, c) => a + c.length * SUBSTRING_DISTANCE_FACTOR, 0);
    matches.push(match);
  }
  matches.sort((a, b) => a.distance - b.distance);
  return matches[0];
}
