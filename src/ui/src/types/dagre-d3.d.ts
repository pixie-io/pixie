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

// dagre-d3.d.ts
// Copied from existing type definition @types/dagre-d3, with some modification
// https://github.com/dagrejs/dagre-d3/issues/339
import * as d3 from 'd3';
import * as dagre from 'dagre';

declare module 'dagre-d3' {

  export const render: { new(): Render };
  export const intersect: {
    [shapeName: string]: (
      node: dagre.Node,
      points: Array<any>,
      point: any,
    ) => void;
  };

  export interface Render {
    arrows(): {
      [arrowStyleName: string]: (
        parent: d3.Selection<d3.BaseType, any, d3.BaseType, any>,
        id: string,
        edge: dagre.Edge,
        type: string,
      ) => void;
    };
    (
      selection: d3.Selection<d3.BaseType, any, d3.BaseType, any>,
      g: dagre.graphlib.Graph,
    ): void;
    shapes(): {
      [shapeStyleName: string]: (
        parent: d3.Selection<d3.BaseType, any, d3.BaseType, any>,
        bbox: any,
        node: dagre.Node,
      ) => void;
    };
  }
}
