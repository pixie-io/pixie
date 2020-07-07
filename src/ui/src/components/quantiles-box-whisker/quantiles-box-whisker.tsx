import * as React from 'react';
import { Vega as ReactVega } from 'react-vega';
import { VisualizationSpec } from 'vega-embed';
import { Handler } from 'vega-tooltip';
import { GaugeLevel } from 'utils/latency';
import {
  createStyles, Theme, useTheme, withStyles, WithStyles,
} from '@material-ui/core/styles';

function getColor(level: GaugeLevel, theme: Theme): string {
  switch (level) {
    case 'low':
      return theme.palette.success.main;
    case 'med':
      return theme.palette.warning.main;
    case 'high':
      return theme.palette.error.main;
    default:
      return theme.palette.text.primary;
  }
}

interface QuantilesBoxWhiskerFields {
  p50: number;
  p90: number;
  p99: number;
  max: number;
  p50Fill: string;
  p90Fill: string;
  p99Fill: string;
  barFill: string;
  whiskerFill: string;
  hoverFill: string;
}

const makeSpec = (fields: QuantilesBoxWhiskerFields): VisualizationSpec => {
  const {
    p50, p90, p99, max, p50Fill, p90Fill, p99Fill, barFill, whiskerFill, hoverFill,
  } = fields;

  return {
    $schema: 'https://vega.github.io/schema/vega/v5.json',
    width: 200,
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
          },
        ],
      },
      {
        name: 'data_1',
        source: 'values',
        transform: [
          {
            type: 'filter',
            expr: 'isValid(datum["whiskerMax"]) && isFinite(+datum["whiskerMax"])',
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
              signal: '"p50: " + format(datum["p50"], ".2f")',
            },
            xc: { scale: 'x', field: 'p50' },
            yc: { signal: 'height', mult: 0.5 },
            height: { value: 14 },
            width: { value: 2 },
          },
          hover: {
            fill: { value: hoverFill },
            fillOpacity: { value: 1 },
            tooltip: {
              signal: '"p50: " + format(datum["p50"], ".2f")',
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
              signal: '"p90: " + format(datum["boxMax"], ".2f")',
            },
            xc: { scale: 'x', field: 'boxMax' },
            yc: { signal: 'height', mult: 0.5 },
            height: { value: 14 },
            width: { value: 2 },
          },
          hover: {
            fill: { value: hoverFill },
            fillOpacity: { value: 1 },
            tooltip: {
              signal: '"p90: " + format(datum["boxMax"], ".2f")',
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
              signal: '"p99: " + format(datum["whiskerMax"], ".2f")',
            },
            xc: { scale: 'x', field: 'whiskerMax' },
            yc: { signal: 'height', mult: 0.5 },
            height: { value: 14 },
            width: { value: 2 },
          },
          hover: {
            fill: { value: hoverFill },
            fillOpacity: { value: 1 },
            tooltip: {
              signal: '"p99: " + format(datum["whiskerMax"], ".2f")',
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
        range: [0, {
          signal: 'width',
        }],
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
  };
};

const styles = (theme: Theme) => createStyles({
  '@global': {
    // This style is used to override vega-tooltip default style.
    // ...custom-theme maps to theme: 'custom' in the options below.
    // This mirrors the default style in our existing Material UI tooltips for consistency.
    '#vg-tooltip-element.vg-tooltip.custom-theme': {
      borderWidth: 0,
      color: '#fff',
      padding: '4px 8px',
      fontSize: '0.625rem',
      maxWidth: 300,
      wordWrap: 'break-word',
      fontFamily: 'Roboto',
      fontWeight: 500,
      lineHeight: '1.4em',
      borderRadius: '4px',
      backgroundColor: '#616161',
      opacity: 0.9,
    },
  },
  vegaWrapper: {
    // It seems like ReactVega automatically inserts 5 px padding on the bottom of charts,
    // and there isn't a clear way to turn this off.
    marginTop: 5,
  },
  low: {
    textAlign: 'right',
    width: 100,
    marginLeft: 5,
    marginRight: 10,
    color: getColor('low', theme),
  },
  med: {
    textAlign: 'right',
    width: 100,
    marginLeft: 5,
    marginRight: 10,
    color: getColor('med', theme),
  },
  high: {
    textAlign: 'right',
    width: 100,
    marginLeft: 5,
    marginRight: 10,
    color: getColor('high', theme),
  },
});

interface QuantilesBoxWhiskerProps extends WithStyles<typeof styles> {
  p50: number;
  p90: number;
  p99: number;
  max: number;
  p99Level: GaugeLevel;
}

const QuantilesBoxWhisker = (props: QuantilesBoxWhiskerProps) => {
  const {
    classes, p50, p90, p99, max, p99Level,
  } = props;
  const theme = useTheme();
  // TODO(nserrino): make it possible in the UI to switch between p50, p90, p99 displayed.
  const p50Fill = theme.palette.text.secondary;
  const p90Fill = theme.palette.text.secondary;
  const p99Fill = getColor(p99Level, theme);
  const spec = makeSpec({
    p50,
    p90,
    p99,
    max,
    p50Fill,
    p90Fill,
    p99Fill,
    barFill: theme.palette.primary.main,
    whiskerFill: theme.palette.text.primary,
    hoverFill: theme.palette.secondary.dark,
  });

  const tooltipHandler = new Handler({
    offsetY: -15,
    theme: 'custom',
  }).call;

  return (
    <>
      <ReactVega
        className={classes.vegaWrapper}
        spec={spec}
        actions={false}
        tooltip={tooltipHandler}
      />
      <span className={classes[p99Level]}>
        {`${p99.toFixed(2)}`}
      </span>
    </>
  );
};

export default withStyles(styles)(QuantilesBoxWhisker);
