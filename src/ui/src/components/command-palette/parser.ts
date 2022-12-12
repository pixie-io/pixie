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

export type TokenType = 'none' | 'icon' | 'key' | 'eq' | 'value' | 'typeahead' | 'error';

/** One token in the list as parsed by {@link parse}. */
export interface Token {
  type: TokenType,
  /** Index into the original parsed list where this token exists. */
  index: number,
  /** Text of token as-typed, not as-parsed. */
  text: string;
  /** Value can differ from text, such as when quotes or escape characters are involved. */
  value: string;
  /** Index in orginal input string where this token's text begins. */
  start: number;
  /** Index in original input string after the last character of this token's text. */
  end: number;
  /**
   * If this token is logically connected to another, this refers to it.
   * For the input `foo:bar`, `foo` has a relatedToken of `bar`, while both `:` and `bar` link to `foo`.
   * For the input `foo:` with the caret at the end, the selected token of `:` links to `foo`.
   */
  relatedToken?: Token;
}

/** An individual token that's covered by the selection range of an input given to {@link parse}. */
export interface SelectedToken {
  /** The token whose text is touched by at least one index in the selection range. */
  token: Token;
  /** First index in the token's `text` where the selection exists. */
  selectionStart: number;
  /** Last index in the token's `text` where the selection exists. */
  selectionEnd: number;
}

/** Return value from {@link parse | parse(input, selection)}. The list of tokens, selections, and the key-value map. */
export interface ParseResult {
  /** The original input string before it was parsed. */
  input: string;
  /** String indices into the original input where the caret or selection is. */
  selection: [start: number, end: number];
  /** The sequence of tokens without any trimming. */
  tokens: Token[];
  /** Tokens touched by selection, even on their edges. */
  selectedTokens: SelectedToken[];
  /** If the input string was `foo:bar baz:"some value" rad:`, this is { foo: "bar", baz: "some value", rad: "" }. */
  kvMap: Map<string, string> | null;
}

/**
 * Tries to find the start and end of a quoted string at the beginning of an input string.
 * Given an input like:
 *   `"This string is in \" quotes" something:else`
 * Returns:
 *   { raw: '"This string is in \\" quotes"', val: 'This string is in " quotes' }
 * But if the input doesn't start with a quoted string, returns { raw: '', val: '' } (example: `something:"else here"`).
 *
 * @param input The entire remaining string to parse
 * @returns If the string starts with a quoted substring, { raw: '"that"', val: 'that' }; otherwise { raw: '', val: ''}
 */
function getNextQuotedString(input: string): { raw: string, val: string } {
  if (!input.startsWith('"')) {
    return { raw: '', val: '' };
  }

  let raw = '"';
  let val = '';
  let i = 1;
  for (; i < input.length; i++) {
    const p = input.slice(i - 1, i);
    const c = input.slice(i, i + 1);
    const d = input.slice(i + 1, i + 2) ?? '';
    if (c === '"' ) {
      if (p === '\\') {
        raw += '\\"';
        val += '"';
      } else {
        // Complete
        raw += '"';
        break;
      }
    } else if (c === '\\') {
      // Invalid escape, so stop before consuming it
      if (d !== '"') break;
    } else {
      // Some other character, just consume it
      raw += c;
      val += c;
    }
  }

  return { raw, val };
}

/**
 * Returns a prefix slice of the input string that looks like an unquoted substring.
 * For an input like `foo:bar`, returns `foo`.
 * For an input like `several bare words`, returns `several`.
 * For an input like `unquoted"quoted"`, returns `unquoted`.
 * For an input like `"this doesn't start with an unquoted string"`, returns ``.
 *
 * @param input Entire string to walk through
 * @returns The part of the string before any tokens that don't look like unquoted strings.
 */
function getNextUnquotedString(input: string): string {
  let i = 0;
  for (; i < input.length; i++) {
    const c = input.slice(i, i + 1);
    if (/^[:"\s\\]/.test(c)) break;
  }
  return input.slice(0, Math.max(i, 0));
}

/**
 * Primitive function for input parsing. Just turns it into a list of tokens.
 *
 * As the syntax is basic, so is tokenization: it just checks the beginning of the string for a possible token, saves
 * the first match, and repeats with the rest of the string until there's nothing left to tokenize.
 *
 * An input string like:
 *   `  foo:bar baz:"Spaces and \" internal quotes"`
 * Becomes a list like:
 *   [
 *     { type: 'value', text: 'foo', value: 'foo' start: 0, end: 3, index: 0 },
 *     { type: 'eq', text: ':', value: '', start: 3, end: 4, index: 1 },
 *     { type: 'value', text: 'bar', value: 'bar', ... },
 *     { type: 'none', text: ' ', value: '', ... },
 *     { type: 'value', text: 'baz', ... },
 *     { type: 'eq', ... },
 *     { type: 'value', text: '"Spaces and \\" internal quotes"', value: 'Spaces and " internal quotes', ... },
 *   ]
 *
 * This doesn't handle semantics or connecting related tokens, that's handled in {@link parse}.
 *
 * @param input The full input to process
 * @returns An array of {@link Token} objects to be further processed by {@link parse}.
 */
function tokenize(input: string): Token[] {
  const out: Token[] = [];
  let scanIndex = 0;

  let remain = input;
  let prevLength = NaN;
  while (remain.length) {
    if (remain.length === prevLength) {
      throw new Error('Length stopped changing? ' + JSON.stringify({ remain, prevLength, out }, null, 2));
    }
    prevLength = remain.length;

    const blanks = remain.match(/^\s+/g);
    if (blanks) {
      remain = remain.slice(blanks[0].length);
      out.push({
        type: 'none',
        index: out.length,
        text: blanks[0],
        value: '',
        start: scanIndex,
        end: scanIndex + blanks[0].length,
      });
      scanIndex += blanks[0].length;
      continue;
    }

    const nextChar = remain.slice(0, 1);

    if (nextChar === '\\') {
      const extra = remain.slice(1, 2);
      remain = remain.slice(2);
      out.push({
        type: 'error',
        index: out.length,
        text: '\\' + extra,
        value: '',
        start: scanIndex,
        end: scanIndex + 1,
      });
      scanIndex++;
      continue;
    }

    if (nextChar === ':') {
      remain = remain.slice(1);
      out.push({ type: 'eq', index: out.length, text: ':', value: '', start: scanIndex, end: scanIndex + 1 });
      scanIndex++;
      continue;
    }

    if (nextChar === '"') {
      const { raw, val } = getNextQuotedString(remain);
      const valid = raw.startsWith('"') && raw.endsWith('"') && raw.length >= 2;

      if (valid) {
        // Assume value; semantic parsing will tell keys from values
        remain = remain.slice(raw.length);
        out.push({
          type: 'value',
          index: out.length,
          text: raw,
          value: val,
          start: scanIndex,
          end: scanIndex + raw.length,
        });
        scanIndex += raw.length;
        continue;
      } else {
        const text = getNextUnquotedString(remain.slice(1));
        remain = remain.slice(1 + text.length);
        out.push({
          type: 'error',
          index: out.length,
          text: nextChar + text,
          value: text,
          start: scanIndex,
          end: scanIndex + 1 + text.length,
        });
        scanIndex += 1 + text.length;
        continue;
      }
    }

    const text = getNextUnquotedString(remain);
    remain = remain.slice(text.length);
    out.push({ type: 'value', index: out.length, text, value: text, start: scanIndex, end: scanIndex + text.length });
    scanIndex += text.length;
  }

  // Map each token to its raw text's original positions in the input
  let p = 0;
  for (const t of out) {
    t.start = p;
    t.end = p + t.text.length;
    p += t.text.length;
  }
  return out;
}

/**
 * Finds which tokens, and where, are touched by the selected part of the input string.
 * For example, `foo:bar` with selectionStart 0 and selectionEnd 6 covers all but the `r` character, so you'd get:
 *   [
 *     { selectionStart: 0, selectionEnd: 3, token: { text: 'foo', start: 0, end: 3, ... } },
 *     { selectionStart: 0, selectionEnd: 1, token: { text: ':',   start: 3, end: 4, ... } },
 *     { selectionStart: 0, selectionEnd: 2, token: { text: 'foo', start: 4, end: 7, ... } },
 *   ]
 *
 * @param tokens The output of tokenize(originalInputString)
 * @param selectionStart First index covered by the selection
 * @param selectionEnd Index after the end of the selection
 * @returns The slice of `tokens` that are touched by the selection range, with the exact indices within their raw
 *   `text` strings that the selection touches.
 */
function getSelectedTokens(tokens: Token[], selectionStart: number, selectionEnd: number): SelectedToken[] {
  let seenChars = 0;
  const out = [];

  for (const token of tokens) {
    const prevSeen = seenChars;
    seenChars += token.text.length;

    if (prevSeen > selectionEnd) continue;
    if (prevSeen + token.text.length < selectionStart) continue;
    if (token.type === 'typeahead') continue;

    out.push({
      token,
      selectionStart: Math.min(token.text.length, Math.max(0, selectionStart - prevSeen)),
      selectionEnd: Math.min(token.text.length, selectionEnd - prevSeen),
    });
  }

  return out;
}

// Cache so we don't need to deal with throttling or context management
const RECENT_PARSE_CACHE_LIMIT = 50;
const recentParses: ParseResult[] = [];

// Actual function to run a parse if there's no cached result
function parseNoCache(input: string, selection: [start: number, end: number]): ParseResult {
  const tokens = tokenize(input);

  // Link key-value pairs of tokens together. Show errors if the same key appears twice.
  // TODO(nick): More error checks. `foo:bar :baz` or `foo::bar` don't make sense. Must have `key`, `:`, `val?`.
  const kvMap: Map<string, string> = new Map();
  let i = 0;
  while (i < tokens.length) {
    const next = [i, i + 1, i + 2].map(j => tokens[j]);
    if (next[0].type === 'value' && next[1]?.type === 'eq') {
      const hasValue = next[2]?.type === 'value';

      // If we run into the same key twice, that's an error
      if (kvMap.has(next[0].value)) {
        next[0].type = 'error';
        next[1].type = 'error';
        i += 2;
        if (hasValue) {
          next[2].type = 'error';
          i += 1;
        }
      } else {
        tokens[i].type = 'key'; // Update the syntax highlighting while we're here
        if (hasValue) {
          kvMap.set(next[0].value, next[2].value);
          next[0].relatedToken = next[2];
          next[1].relatedToken = next[0];
          next[2].relatedToken = next[0];
          i += 3;
        } else {
          kvMap.set(next[0].value, '');
          next[1].relatedToken = next[0];
          i += 2;
        }
      }
    } else {
      i += 1;
    }
  }

  return {
    input,
    selection,
    tokens,
    selectedTokens: getSelectedTokens(tokens, Math.min(...selection), Math.max(...selection)),
    kvMap: kvMap.size > 0 ? kvMap : null,
  };
}

/**
 * Given an input for the Command Palette, try to parse it from a `foo:bar baz:"something else"` syntax.
 * Also figures out where the tokens begin and end, what the selection covers, and what the key-to-value map is.
 * If the input is freeform text, this still works, it just doesn't have any key-value mapping.
 * This function caches recent results directly, so it's safe to call repeatedly.
 *
 * @param input The text to parse.
 * @param selection Index of the caret, or the start and end indices of the selection if it exists.
 * @returns {ParseResult}
 */
export function parse(input: string, selection: [start: number, end: number]): ParseResult {
  let out: ParseResult;
  const previousIndex = recentParses.findIndex(
    (p) => p.input === input && p.selection[0] === selection[0] && p.selection[1] === selection[1]);

  if (previousIndex >= 0) {
    out = recentParses[previousIndex];
    recentParses.splice(previousIndex, 1, recentParses[previousIndex]);
  } else {
    out = parseNoCache(input, selection);
    recentParses.push(out);
  }
  while (recentParses.length > RECENT_PARSE_CACHE_LIMIT) recentParses.shift();

  return out;
}
