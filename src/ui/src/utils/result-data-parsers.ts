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

export interface Quantile {
  p50: number;
  p90: number;
  p99: number;
}

export interface ContainerStatus {
  state: string;
  reason: string;
  message: string;
}

export interface PodStatus {
  phase: string;
  reason: string;
  message: string;
  ready: boolean;
}

export interface ScriptReference {
  label: string;
  script: string;
  args: { [arg: string]: string };
}

// u8aToStr converts an array of bytes to a string.
// Used as a utility for managing bytes protobuf fields ie StringColumn.
export function u8aToStr(u8a: Uint8Array): string {
  return new TextDecoder('utf-8').decode(u8a);
}

// stringToU8a converts a string to an array of bytes.
// Used as a utility for managing bytes protobuf fields ie StringColumn.
export function strToU8a(s: string): Uint8Array {
  // Why do we wrap with `Uint8Array.from`?
  // google-protobuf checks the Uint8Array type by checking the constructor type,
  // so we must cast to make it work.
  return Uint8Array.from(new TextEncoder().encode(s));
}

// Parses a JSON string as a quantile, so that downstream sort and renderers don't
// have to reparse the JSON every time they handle a quantiles value.
function parseQuantile(val: any): Quantile {
  try {
    const parsed = JSON.parse(val);
    const { p50, p90, p99 } = parsed;
    return { p50, p90, p99 };
  } catch (error) {
    return null;
  }
}

function parseContainerStatus(val: any): ContainerStatus {
  try {
    const parsed = JSON.parse(val);
    const { state, reason, message } = parsed;
    return { state, reason, message };
  } catch (error) {
    return null;
  }
}

function parsePodStatus(val: any): PodStatus {
  try {
    const parsed = JSON.parse(val);
    const {
      phase, reason, message, ready,
    } = parsed;
    return {
      phase, reason, message, ready,
    };
  } catch (error) {
    return null;
  }
}

function parseScriptReference(val: any): ScriptReference {
  try {
    const parsed = JSON.parse(val);
    const { label, script, args } = parsed;
    return { label, script, args };
  } catch (error) {
    return null;
  }
}

// Parses the rows based on their semantic type.
export function parseRows(semanticTypeMap: Map<string, SemanticType>, rows: any[]): any[] {
  const parsers = new Map();
  semanticTypeMap.forEach((semanticType, dataKey) => {
    switch (semanticType) {
      case SemanticType.ST_CONTAINER_STATUS:
        parsers.set(dataKey, parseContainerStatus);
        break;
      case SemanticType.ST_POD_STATUS:
        parsers.set(dataKey, parsePodStatus);
        break;
      case SemanticType.ST_QUANTILES:
      case SemanticType.ST_DURATION_NS_QUANTILES:
        parsers.set(dataKey, parseQuantile);
        break;
      case SemanticType.ST_SCRIPT_REFERENCE:
        parsers.set(dataKey, parseScriptReference);
        break;
      default:
        break;
    }
  });

  if (parsers.size === 0) {
    return rows;
  }

  return rows.map((row) => {
    const newValues = {};
    parsers.forEach((parser, dataKey) => {
      newValues[dataKey] = parser(row[dataKey]);
    });
    return {
      ...row,
      ...newValues,
    };
  });
}
