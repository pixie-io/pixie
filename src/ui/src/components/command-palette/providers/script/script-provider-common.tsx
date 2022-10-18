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

import * as React from 'react';

import { gql } from '@apollo/client';
import { Box, Typography } from '@mui/material';

import { PixieAPIClient } from 'app/api';
import { parse, ParseResult, Token } from 'app/components/command-palette/parser';
import { CommandCompletion } from 'app/components/command-palette/providers/command-provider';
import { GQLAutocompleteEntityKind, GQLAutocompleteFieldResult } from 'app/types/schema';

export type CompletionSet = { completions: CommandCompletion[], hasAdditionalMatches: boolean };

const needQuoteRe = new RegExp('[\\s"]+');
export function quoteIfNeeded(input: string): string {
  // If the input is already quoted, assume it doesn't need to be wrapped again.
  // Technically, the string `"foo"bar"` would be illegal but still pass here, but we aren't doing that.
  if (input.startsWith('"') && input.endsWith('"') && input.length >= 2) return input;
  return needQuoteRe.test(input) ? `"${input.replace(/"/g, '\\"')}"` : input;
}

export const CompletionLabel = React.memo<{
  icon: React.ReactNode,
  input: string,
  highlights: number[],
}>(({ icon, input, highlights }) => {
  const outs: React.ReactNode[] = [];
  for (let i = 0; i < input.length; i++) {
    if (highlights.includes(i)) outs.push(<strong key={i}>{input.substring(i, i + 1)}</strong>);
    else outs.push(<span key={i}>{input.substring(i, i + 1)}</span>);
  }
  return (
    // eslint-disable-next-line react-memo/require-usememo
    <Box sx={{ display: 'flex', gap: 1, flexFlow: 'row nowrap' }}>
      {icon}
      <span>{outs}</span>
    </Box>
  );
});
CompletionLabel.displayName = 'CompletionLabel';

export const CompletionDescription = React.memo<{
  body: React.ReactNode,
  title?: React.ReactNode,
  hint?: React.ReactNode,
}>(({ body, title, hint }) => {
  return (
    <>
      {title && <Typography variant='h2'>{title}</Typography>}
      {/* eslint-disable-next-line react-memo/require-usememo */}
      {hint && <Typography variant='subtitle2' sx={{ fontStyle: 'italic' }}>({hint})</Typography>}
      {/* eslint-disable-next-line react-memo/require-usememo */}
      <Typography variant='body1' sx={{ mt: (title || hint) ? 1 : 0 }}>{body}</Typography>
    </>
  );
});
CompletionDescription.displayName = 'CompletionDescription';


export async function getFieldSuggestions(
  search: string,
  kind: GQLAutocompleteEntityKind,
  clusterUID: string,
  client: PixieAPIClient,
): Promise<GQLAutocompleteFieldResult> {
  const query = client.getCloudClient().graphQL.query<{ autocompleteField: GQLAutocompleteFieldResult }>({
    query: gql`
      query getCompletions($input: String, $kind: AutocompleteEntityKind, $clusterUID:String) {
        autocompleteField(input: $input, fieldType: $kind, clusterUID: $clusterUID) {
          suggestions {
            name
            description
            matchedIndexes
            state
          }
          hasAdditionalMatches
        }
      }
    `,
    fetchPolicy: 'no-cache',
    variables: {
      input: search,
      kind,
      clusterUID,
    },
  });

  try {
    const { data: { autocompleteField } } = await query;
    return autocompleteField;
  } catch (err) {
    console.warn('Something went wrong fetching suggestions, likely transient:', err);
    return { suggestions: [], hasAdditionalMatches: false };
  }
}

/**
 * If the input didn't have key:value pairs or if the selection covered more than one pair, transforms the parse to act
 * like the input was one huge quoted string.
 */
export function combineParseIfOneBigString(parsed: ParseResult): ParseResult {
  const selectedKeys = parsed.selectedTokens.filter(t => t.token.type === 'key');
  const selectedValues = parsed.selectedTokens.filter(t => t.token.type === 'value');

  const tooManySelections = selectedKeys.length + selectedValues.length > 1;
  const inputIsOneLongString = parsed.tokens.every(({ type }) => type === 'value' || type === 'none');

  if (tooManySelections && inputIsOneLongString) {
    const newInput = quoteIfNeeded(parsed.tokens.map(({ text }) => text).join(''));
    return parse(newInput, [newInput.length, newInput.length]);
  }

  return parsed;
}

/**
 * Replace selected token (or the full key/value token pair if it's part of one) with the given key/value pair.
 * If the given key already exists in the parse and it isn't selected, replace that instead.
 */
export function getOnSelectSetKeyVal(
  parsed: ParseResult,
  selectedToken: Token, // Which of the selected tokens to replace (may be key or value)
  newKey: string, // Must not be empty
  newVal: string, // May be empty string (for example, adding a new key without a value)
  addSpaceAfter = true, // If true, this adds a space afterward to immediately start to suggest the next thing.
): CommandCompletion['onSelect'] {
  const matchingKey = parsed.tokens.find((t) => t.type === 'key' && t.value === newKey) ?? null;
  const extraSpace = addSpaceAfter ? ' ' : '';

  return () => {
    // If there's already a matching key in the parse, pretend we had selected that.
    // Then, replace the key:value pair at that location (or the bare val or the `key:`) with the newKey/newVal combo.
    const tok = matchingKey ?? selectedToken;
    const relatives = parsed.tokens.filter(t => t === tok || t.relatedToken === tok || tok.relatedToken === t);

    const sliceLeft = Math.min(...relatives.map(r => r.index ?? Infinity));
    const sliceRight = Math.max(...relatives.map(r => r.index ?? -Infinity));

    const prefix = parsed.tokens.slice(0, sliceLeft).map(t => t.text).join('').trim();
    const infix = `${newKey}:${newVal ? newVal + extraSpace : ''}`; // To immediately get more suggestions
    const suffix = parsed.tokens.slice(sliceRight + 1).map(t => t.text).join('').trim();

    return [
      [prefix, infix, suffix].join(' ').trimLeft(),
      [prefix, infix].join(' ').trimLeft().length,
    ];
  };
}

export function getStringHighlightSortFn(search: string): (a: string, am: number[], b: string, bm: number[]) => number {
  return (a, am, b, bm) => {
    const isSubstringDistance = Number(b.includes(search)) - Number(a.includes(search));

    // More characters matching is the most important part of match strength
    const matchDistance = bm.length - am.length;

    // If two suggestions matched the same number of characters, the one with less gap between matches is better
    const spread = (m: number[]) => m.reduce((o, c, i) => o + c - (m[i - 1] ?? 0), 0);
    const matchSpreadDistance = spread(am) - spread(bm);

    // If there's still a tie, just sort alphabetically
    const alphaDistance = a.localeCompare(b);

    // Combine the scores by priority to sort; stay in range [-1, 0, 1] so it's easier to combine with other sorts
    return Math.sign((isSubstringDistance * 1e3)
      + (matchDistance * 1e2)
      + (matchSpreadDistance * 10)
      + alphaDistance);
  };
}

/**
 * If the selection covers exactly one key:value pair (even with an empty value), return those tokens; or else nothing.
 * Selecting more than one key, more than one value, or one of each but they're from different pairs, is invalid.
 * @param parsed
 * @returns
 */
export function getSelectedKeyAndValueToken(parsed: ParseResult): { keyToken?: Token, valueToken?: Token } {
  const selectedKeys = parsed.selectedTokens.filter(t => t.token.type === 'key');
  const selectedEq = parsed.selectedTokens.filter(t => t.token.type === 'eq');
  const selectedValues = parsed.selectedTokens.filter(t => t.token.type === 'value');

  if (selectedKeys.length > 1 || selectedValues.length > 1) return { keyToken: null, valueToken: null };

  // A key token sets `relatedToken` to the value token, if present. Both eq and value tokens link to their key token.
  const keyToken = selectedKeys[0]?.token
    ?? selectedValues[0]?.token.relatedToken
    ?? selectedEq[0]?.token.relatedToken
    ?? null;

  const valueToken = selectedValues[0]?.token
    ?? selectedKeys[0]?.token.relatedToken
    ?? selectedEq[0]?.token.relatedToken?.relatedToken
    ?? null;

  if (keyToken && valueToken && valueToken.relatedToken !== keyToken) return { keyToken: null, valueToken: null };

  return { keyToken, valueToken };
}
