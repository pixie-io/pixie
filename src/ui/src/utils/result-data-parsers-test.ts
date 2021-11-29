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

import { SemanticType } from 'app/types/generated/vizierapi_pb';

import { parseRows } from './result-data-parsers';

describe('parseRows', () => {
  it('correctly parses a row with quantiles', () => {
    const semanticTypes = new Map([
      ['quantileCol', SemanticType.ST_QUANTILES],
      ['nonQuantileCol', SemanticType.ST_NONE],
    ]);

    expect(parseRows(semanticTypes, [
      { quantileCol: '{"p10": 123, "p50": 345, "p90": 456, "p99": 789}', nonQuantileCol: '6' },
      { quantileCol: '{"p10": 123, "p50": 789, "p90": 1010, "p99": 2000}', nonQuantileCol: '5' },
    ]))
      .toStrictEqual([
        { quantileCol: { p50: 345, p90: 456, p99: 789 }, nonQuantileCol: '6' },
        { quantileCol: { p50: 789, p90: 1010, p99: 2000 }, nonQuantileCol: '5' },
      ]);
  });

  it('correctly parses a row with pod statuses', () => {
    const semanticTypes = new Map([
      ['status', SemanticType.ST_POD_STATUS],
    ]);

    expect(parseRows(semanticTypes, [
      { status: '{"phase": "RUNNING", "reason": "foo", "message": "bar", "ready": true}' },
      { status: 'notcorrect' },
    ]))
      .toStrictEqual([
        {
          status: {
            phase: 'RUNNING', reason: 'foo', message: 'bar', ready: true,
          },
        },
        { status: null },
      ]);
  });

  it('correctly parses a row with container statuses', () => {
    const semanticTypes = new Map([
      ['status', SemanticType.ST_CONTAINER_STATUS],
    ]);

    expect(parseRows(semanticTypes, [
      { status: '{"state": "RUNNING", "reason": "foo", "message": "bar"}' },
      { status: 'notcorrect' },
    ]))
      .toStrictEqual([
        { status: { state: 'RUNNING', reason: 'foo', message: 'bar' } },
        { status: null },
      ]);
  });

  it('correctly parses a row with script references', () => {
    const semanticTypes = new Map([
      ['ref', SemanticType.ST_SCRIPT_REFERENCE],
    ]);

    expect(parseRows(semanticTypes, [
      { ref: '{"label": "clickable", "script": "px/namespaces", "args": {"bar": "abc"}}' },
      { ref: 'notcorrect' },
    ]))
      .toStrictEqual([
        { ref: { label: 'clickable', script: 'px/namespaces', args: { bar: 'abc' } } },
        { ref: null },
      ]);
  });

  it('correctly returns unchanged rows with no types in need of special parsing', () => {
    const semanticTypes = new Map([
      ['quantileCol', SemanticType.ST_NONE],
      ['nonQuantileCol', SemanticType.ST_NONE],
    ]);

    const rows = [
      { quantileCol: '{"p10": 123, "p50": 345, "p90": 456, "p99": 789}', nonQuantileCol: '6' },
      { quantileCol: '{"p10": 123, "p50": 789, "p90": 1010, "p99": 2000}', nonQuantileCol: '5' },
    ];
    expect(parseRows(semanticTypes, rows)).toBe(rows);
  });
});
