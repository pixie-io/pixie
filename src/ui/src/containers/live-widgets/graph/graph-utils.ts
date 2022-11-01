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

import { Theme } from '@mui/material/styles';
import { Options } from 'vis-network/standalone';

import { DataType, Relation, SemanticType } from 'app/types/generated/vizierapi_pb';

import * as podSVG from './pod.svg';
import * as svcSVG from './svc.svg';

export const LABEL_OPTIONS = {
  label: {
    min: 8,
    max: 20,
    maxVisible: 20,
  },
};

export interface ColInfo {
  type: DataType;
  semType: SemanticType;
  name: string;
}

export function getGraphOptions(theme: Theme, edgeLength: number): Options {
  return {
    clickToUse: false,
    layout: {
      randomSeed: 10,
      improvedLayout: false,
    },
    physics: {
      solver: 'forceAtlas2Based',
      forceAtlas2Based: {
        gravitationalConstant: -50,
        springLength: edgeLength > 0 ? edgeLength : 100,
      },
      hierarchicalRepulsion: {
        nodeDistance: 100,
      },
      stabilization: {
        iterations: 250,
        updateInterval: 10,
      },
    },
    edges: {
      smooth: false,
      scaling: {
        max: 5,
      },
      arrows: {
        to: {
          enabled: true,
          type: 'arrow',
          scaleFactor: 0.5,
        },
      },
    },
    nodes: {
      borderWidth: 0.5,
      scaling: LABEL_OPTIONS,
      font: {
        face: 'Roboto',
        color: theme.palette.text.primary,
        align: 'left',
      },
    },
  };
}

const semTypeToIcon = {
  [SemanticType.ST_SERVICE_NAME]: svcSVG,
  [SemanticType.ST_POD_NAME]: podSVG,
};

export function semTypeToShapeConfig(st: SemanticType): any {
  if (semTypeToIcon[st]) {
    const icon = semTypeToIcon[st];
    return {
      shape: 'image',
      image: {
        selected: icon,
        unselected: icon,
      },
    };
  }
  return {
    shape: 'dot',
  };
}

export function getNamespaceFromEntityName(val: string): string {
  return val.split('/')[0];
}

const nsPerMs = 1_000_000;

/**
 * Colors latency by how high it is.
 * Higher latency gets more concerning colors.
 */
export function getColorForLatency(nanoseconds: number, theme: Theme): string {
  if (nanoseconds < 100 * nsPerMs) {
    return theme.palette.success.dark;
  }
  return nanoseconds > 200 * nsPerMs ? theme.palette.error.main : theme.palette.warning.main;
}

/**
 * Colors an ratio in between 0 and 1 (inclusive) by how high it is.
 * Higher error rates get more concerning colors.
 */
export function getColorForErrorRate(rate: number, theme: Theme): string {
  if (rate < 0.01) {
    return theme.palette.success.dark;
  }
  return rate > 0.02 ? theme.palette.error.main : theme.palette.warning.main;
}

export function colInfoFromName(relation: Relation, name: string): ColInfo {
  const cols = relation.getColumnsList();
  for (let i = 0; i < cols.length; i++) {
    if (cols[i].getColumnName() === name) {
      return {
        name,
        type: cols[i].getColumnType(),
        semType: cols[i].getColumnSemanticType(),
      };
    }
  }
  return undefined;
}
