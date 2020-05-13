import {addLayout} from './layout';
import {TABLE_DISPLAY_TYPE, Vis} from './vis';

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
      { x: 0, y: 0, w: 6, h: 3 },
      { x: 6, y: 0, w: 6, h: 3 },
      { x: 0, y: 3, w: 6, h: 3 },
    ];

    const newVis = addLayout(visSpec);
    expect(newVis).toStrictEqual({
      ...visSpec,
      widgets: visSpec.widgets.map((widget, i) => {
        return {
          ...widget,
          position: expectedPositions[i],
        };
      }),
    });
  });

  it('keeps a grid when specified', () => {
    const positions = [
      { x: 0, y: 0, w: 6, h: 3 },
      { x: 6, y: 0, w: 6, h: 3 },
      { x: 0, y: 3, w: 6, h: 3 },
    ];

    const inputVis = {
      ...visSpec,
      widgets: visSpec.widgets.map((widget, i) => {
        return {
          ...widget,
          position: positions[i],
        };
      }),
    };
    const newVis = addLayout(inputVis);
    expect(newVis).toEqual(inputVis);
  });
});
