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
import { Vega } from 'react-vega';
import { VisualizationSpec } from 'vega-embed';
import { Handler } from 'vega-tooltip';
import {
  Theme,
  useTheme,
  makeStyles,
} from '@material-ui/core/styles';
import { createStyles } from '@material-ui/styles';

interface QuantilesBoxWhiskerFields {
  p50: number;
  p90: number;
  p99: number;
  max: number;
  p50Display: string;
  p90Display: string;
  p99Display: string;
  p50Fill: string;
  p90Fill: string;
  p99Fill: string;
  p50HoverFill: string;
  p90HoverFill: string;
  p99HoverFill: string;
  barFill: string;
  whiskerFill: string;
}

const makeSpec = (fields: QuantilesBoxWhiskerFields): VisualizationSpec => {
  const {
    p50,
    p90,
    p99,
    max,
    p50Display,
    p90Display,
    p99Display,
    p50Fill,
    p90Fill,
    p99Fill,
    p50HoverFill,
    p90HoverFill,
    p99HoverFill,
    barFill,
    whiskerFill,
  } = fields;

  return {
    $schema: 'https://vega.github.io/schema/vega/v5.json',
    style: 'cell',
    data: [
      {
        name: 'values',
        values: [
          {
            p50,
            boxMin: 0,
            boxMax: p90,
            whiskerMin: 0,
            whiskerMax: p99,
            p50Display,
            p90Display,
            p99Display,
          },
        ],
      },
      {
        name: 'data_1',
        source: 'values',
        transform: [
          {
            type: 'filter',
            expr:
              'isValid(datum["whiskerMax"]) && isFinite(+datum["whiskerMax"])',
          },
        ],
      },
      {
        name: 'data_2',
        source: 'values',
        transform: [
          {
            type: 'filter',
            expr: 'isValid(datum["p50"]) && isFinite(+datum["p50"])',
          },
        ],
      },
    ],
    marks: [
      {
        name: 'whisker',
        type: 'rule',
        style: ['rule', 'boxplot-rule'],
        aria: false,
        from: { data: 'values' },
        encode: {
          update: {
            stroke: { value: whiskerFill },
            x: { scale: 'x', field: 'boxMax' },
            x2: { scale: 'x', field: 'whiskerMax' },
            y: { signal: 'height', mult: 0.5 },
          },
        },
      },
      {
        name: 'bar',
        type: 'rect',
        style: ['bar', 'boxplot-box'],
        aria: false,
        from: { data: 'values' },
        encode: {
          update: {
            strokeWidth: { value: 0 },
            opacity: { value: 0.6 },
            fill: { value: barFill },
            x: { scale: 'x', field: 'boxMin' },
            x2: { scale: 'x', field: 'boxMax' },
            yc: { signal: 'height', mult: 0.5 },
            height: { value: 14 },
          },
        },
      },
      {
        name: 'p50',
        type: 'rect',
        style: ['tick'],
        from: { data: 'data_2' },
        encode: {
          update: {
            opacity: { value: 0.7 },
            fill: { value: p50Fill },
            ariaRoleDescription: { value: 'tick' },
            description: {
              signal: '"p50: " + datum["p50Display"]',
            },
            xc: { scale: 'x', field: 'p50' },
            yc: { signal: 'height', mult: 0.5 },
            height: { value: 14 },
            width: { value: 2 },
          },
          hover: {
            fill: { value: p50HoverFill },
            fillOpacity: { value: 1 },
            tooltip: {
              signal: '"p50: " + datum["p50Display"]',
            },
          },
        },
      },
      {
        name: 'p90',
        type: 'rect',
        style: ['tick'],
        from: { data: 'data_2' },
        encode: {
          update: {
            opacity: { value: 0.7 },
            fill: { value: p90Fill },
            ariaRoleDescription: { value: 'tick' },
            description: {
              signal: '"p90: " + datum["p90Display"]',
            },
            xc: { scale: 'x', field: 'boxMax' },
            yc: { signal: 'height', mult: 0.5 },
            height: { value: 14 },
            width: { value: 2 },
          },
          hover: {
            fill: { value: p90HoverFill },
            fillOpacity: { value: 1 },
            tooltip: {
              signal: '"p90: " + datum["p90Display"]',
            },
          },
        },
      },
      {
        name: 'p99',
        type: 'rect',
        style: ['tick'],
        from: { data: 'data_1' },
        encode: {
          update: {
            opacity: { value: 0.7 },
            fill: { value: p99Fill },
            ariaRoleDescription: { value: 'tick' },
            description: {
              signal: '"p99: " + datum["p99Display"]',
            },
            xc: { scale: 'x', field: 'whiskerMax' },
            yc: { signal: 'height', mult: 0.5 },
            height: { value: 14 },
            width: { value: 2 },
          },
          hover: {
            fill: { value: p99HoverFill },
            fillOpacity: { value: 1 },
            tooltip: {
              signal: '"p99: " + datum["p99Display"]',
            },
          },
        },
      },
    ],
    scales: [
      {
        name: 'x',
        type: 'sqrt',
        domain: [0, max],
        range: [
          0,
          {
            signal: 'width',
          },
        ],
        nice: true,
        zero: true,
      },
    ],
    config: {
      style: {
        cell: {
          stroke: 'transparent',
        },
      },
    },
    signals: [
      {
        name: 'p50Click',
        on: [
          {
            events: { type: 'click', markname: 'p50' },
            update: '{}',
          },
        ],
      },
      {
        name: 'p90Click',
        on: [
          {
            events: { type: 'click', markname: 'p90' },
            update: '{}',
          },
        ],
      },
      {
        name: 'p99Click',
        on: [
          {
            events: { type: 'click', markname: 'p99' },
            update: '{}',
          },
        ],
      },
      {
        name: 'width',
        init: 'isFinite(containerSize()[0]) ? containerSize()[0] : 200',
        on: [
          {
            update: 'isFinite(containerSize()[0]) ? containerSize()[0] : 200',
            events: 'window: resize',
          },
        ],
      },
    ],
  };
};

const useStyles = makeStyles((theme: Theme) => createStyles({
  '@global': {
    // This style is used to override vega-tooltip default style.
    // ...custom-theme maps to theme: 'custom' in the options below.
    // This mirrors the default style in our existing Material UI tooltips for consistency.
    '#vg-tooltip-element.vg-tooltip.custom-theme': {
      borderWidth: 0,
      color: theme.palette.foreground.white,
      padding: '4px 8px',
      fontSize: '0.625rem',
      maxWidth: 300,
      wordWrap: 'break-word',
      fontFamily: 'Roboto',
      fontWeight: 500,
      lineHeight: '1.4em',
      borderRadius: '4px',
      backgroundColor: theme.palette.foreground.grey1,
      opacity: 0.9,
    },
  },
  root: {
    display: 'flex',
    alignItems: 'center',
  },
  vegaWrapper: {
    paddingRight: theme.spacing(1),
    flex: '0 1 100%',
  },
  vega: {
    width: '100%',
  },
  label: {
    textAlign: 'right',
    justifyContent: 'flex-end',
    width: '4rem',
    flex: '0 0 auto',
    marginLeft: 5,
    marginRight: 10,
  },
}));

export type SelectedPercentile = 'p50' | 'p90' | 'p99';

interface QuantilesBoxWhiskerProps {
  p50: number;
  p90: number;
  p99: number;
  max: number;
  p50Display: string;
  p90Display: string;
  p99Display: string;
  p50HoverFill: string;
  p90HoverFill: string;
  p99HoverFill: string;
  selectedPercentile: SelectedPercentile;
  // Function to call when the selected percentile is updated.
  onChangePercentile?: (percentile: SelectedPercentile) => void;
}

export const QuantilesBoxWhisker: React.FC<QuantilesBoxWhiskerProps> = (props) => {
  const {
    p50,
    p90,
    p99,
    max,
    p50HoverFill,
    p90HoverFill,
    p99HoverFill,
    p50Display,
    p90Display,
    p99Display,
    selectedPercentile,
    onChangePercentile,
  } = props;

  const classes = useStyles();
  const theme = useTheme();
  let p50Fill = theme.palette.text.secondary;
  let p90Fill = theme.palette.text.secondary;
  let p99Fill = theme.palette.text.secondary;
  let selectedPercentileDisplay;
  let selectedPercentileFill;

  const changePercentileIfDifferent = (percentile: SelectedPercentile) => {
    if (percentile !== selectedPercentile) {
      onChangePercentile(percentile);
    }
  };

  switch (selectedPercentile) {
    case 'p50': {
      p50Fill = p50HoverFill;
      selectedPercentileDisplay = p50Display;
      selectedPercentileFill = p50HoverFill;
      break;
    }
    case 'p90': {
      p90Fill = p90HoverFill;
      selectedPercentileDisplay = p90Display;
      selectedPercentileFill = p90HoverFill;
      break;
    }
    case 'p99':
    default: {
      p99Fill = p99HoverFill;
      selectedPercentileDisplay = p99Display;
      selectedPercentileFill = p99HoverFill;
    }
  }

  // Vega's chart sets an explicit width on its canvas, even in responsive mode. It doesn't figure out the right width
  // when it's in a flex child and tries to grow too large. Telling it how wide its container is fixes this.
  const spec = makeSpec({
    p50,
    p90,
    p99,
    max,
    p50Display,
    p90Display,
    p99Display,
    p50Fill,
    p90Fill,
    p99Fill,
    p50HoverFill,
    p90HoverFill,
    p99HoverFill,
    barFill: theme.palette.primary.dark,
    whiskerFill: theme.palette.text.primary,
  });

  const tooltipHandler = new Handler({
    offsetY: -15,
    theme: 'custom',
  }).call;

  return (
    <div className={classes.root}>
      <div className={classes.vegaWrapper}>
        <Vega
          className={classes.vega}
          signalListeners={{
            p50Click: () => changePercentileIfDifferent('p50'),
            p90Click: () => changePercentileIfDifferent('p90'),
            p99Click: () => changePercentileIfDifferent('p99'),
          }}
          spec={spec}
          actions={false}
          tooltip={tooltipHandler}
        />
      </div>
      <span className={classes.label} style={{ color: selectedPercentileFill }}>
        {selectedPercentileDisplay}
      </span>
    </div>
  );
};
