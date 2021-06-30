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

import { addLayout, toLayout } from './layout';
import { TABLE_DISPLAY_TYPE, Vis } from './vis';

const visSpec: Vis = {
  variables: [],
  globalFuncs: [],
  widgets: [
    {
      name: 'latency',
      func: {
        name: 'get_latency',
        args: [],
      },
      displaySpec: {
        '@type': TABLE_DISPLAY_TYPE,
      },
    },
    {
      name: 'error_rate',
      func: {
        name: 'get_error_rate',
        args: [],
      },
      displaySpec: {
        '@type': TABLE_DISPLAY_TYPE,
      },
    },
    {
      name: 'rps',
      func: {
        name: 'get_error_rate',
        args: [],
      },
      displaySpec: {
        '@type': TABLE_DISPLAY_TYPE,
      },
    },
  ],
};

describe('BuildLayout', () => {
  it('tiles a grid', () => {
    const expectedPositions = [
      {
        x: 0, y: 0, w: 6, h: 3,
      },
      {
        x: 6, y: 0, w: 6, h: 3,
      },
      {
        x: 0, y: 3, w: 6, h: 3,
      },
    ];

    const newVis = addLayout(visSpec);
    expect(newVis).toStrictEqual({
      ...visSpec,
      widgets: visSpec.widgets.map((widget, i) => ({
        ...widget,
        position: expectedPositions[i],
      })),
    });
  });

  it('keeps a grid when specified', () => {
    const positions = [
      {
        x: 0, y: 0, w: 6, h: 3,
      },
      {
        x: 6, y: 0, w: 6, h: 3,
      },
      {
        x: 0, y: 3, w: 6, h: 3,
      },
    ];

    const inputVis = {
      ...visSpec,
      widgets: visSpec.widgets.map((widget, i) => ({
        ...widget,
        position: positions[i],
      })),
    };
    const newVis = addLayout(inputVis);
    expect(newVis).toEqual(inputVis);
  });
});

describe('toLayout', () => {
  it('generates the expected non-mobile layout', () => {
    const positions = [
      {
        x: 0, y: 0, w: 6, h: 3,
      },
      {
        x: 6, y: 0, w: 6, h: 3,
      },
      {
        x: 0, y: 3, w: 6, h: 3,
      },
    ];

    const widgets = visSpec.widgets.map((widget, i) => ({
      ...widget,
      position: positions[i],
    }));

    // Delete the name so we can test the default table naming.
    delete widgets[0].name;

    const resultLayout = toLayout(widgets, false, null);

    expect(resultLayout).toStrictEqual([
      {
        i: 'widget_0_0',
        x: 0,
        y: 0,
        h: 3,
        w: 6,
        minH: 2,
        minW: 2,
      },
      {
        i: 'error_rate',
        x: 6,
        y: 0,
        h: 3,
        w: 6,
        minH: 2,
        minW: 2,
      },
      {
        i: 'rps',
        x: 0,
        y: 3,
        h: 3,
        w: 6,
        minH: 2,
        minW: 2,
      },
    ]);
  });

  it('generates the expected non-mobile layout with a missing position', () => {
    const positions = [
      null,
      {
        x: 6, y: 0, w: 6, h: 3,
      },
      {
        x: 0, y: 3, w: 6, h: 3,
      },
    ];

    const widgets = visSpec.widgets.map((widget, i) => ({
      ...widget,
      position: positions[i],
    }));

    // Delete the name so we can test the default table naming.
    delete widgets[0].name;

    const resultLayout = toLayout(widgets, false, null);

    expect(resultLayout).toStrictEqual([
      {
        i: 'widget_0_0',
        x: 0,
        y: 0,
        h: 0,
        w: 0,
        minH: 2,
        minW: 2,
      },
      {
        i: 'error_rate',
        x: 6,
        y: 0,
        h: 3,
        w: 6,
        minH: 2,
        minW: 2,
      },
      {
        i: 'rps',
        x: 0,
        y: 3,
        h: 3,
        w: 6,
        minH: 2,
        minW: 2,
      },
    ]);
  });

  it('generates the expected mobile layout', () => {
    const positions = [
      {
        x: 0, y: 0, w: 6, h: 3,
      },
      {
        x: 6, y: 0, w: 6, h: 3,
      },
      {
        x: 0, y: 3, w: 6, h: 3,
      },
    ];

    const widgets = visSpec.widgets.map((widget, i) => ({
      ...widget,
      position: positions[i],
    }));

    // Delete the name so we can test the default table naming.
    delete widgets[0].name;
    const resultLayout = toLayout(widgets, true, null);

    expect(resultLayout).toStrictEqual([
      {
        i: 'widget_0_0',
        x: 0,
        y: 0,
        h: 2,
        w: 4,
      },
      {
        i: 'error_rate',
        x: 0,
        y: 2,
        h: 2,
        w: 4,
      },
      {
        i: 'rps',
        x: 0,
        y: 4,
        h: 2,
        w: 4,
      },
    ]);
  });

  it('generates the expected mobile layout when widgets are declared in non-row-major order', () => {
    const positions = [
      {
        x: 6, y: 0, w: 6, h: 3,
      },
      {
        x: 0, y: 3, w: 6, h: 3,
      },
      {
        x: 0, y: 0, w: 6, h: 3,
      },
    ];

    const widgets = visSpec.widgets.map((widget, i) => ({
      ...widget,
      position: positions[i],
    }));

    // Delete the name so we can test the default table naming.
    delete widgets[0].name;

    const resultLayout = toLayout(widgets, true, null);

    expect(resultLayout).toStrictEqual([
      {
        i: 'widget_0_0',
        x: 0,
        y: 2,
        h: 2,
        w: 4,
      },
      {
        i: 'error_rate',
        x: 0,
        y: 4,
        h: 2,
        w: 4,
      },
      {
        i: 'rps',
        x: 0,
        y: 0,
        h: 2,
        w: 4,
      },
    ]);
  });
});
