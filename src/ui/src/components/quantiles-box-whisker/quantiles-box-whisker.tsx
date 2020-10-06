import * as React from 'react';
import { Vega as ReactVega } from 'react-vega';
import { VisualizationSpec } from 'vega-embed';
import { Handler } from 'vega-tooltip';
import { GaugeLevel } from 'utils/metric-thresholds';
import {
  createStyles, Theme, useTheme, withStyles, WithStyles,
} from '@material-ui/core/styles';
import clsx from 'clsx';

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
    p50, p90, p99, max, p50Display, p90Display, p99Display, p50Fill, p90Fill, p99Fill,
    p50HoverFill, p90HoverFill, p99HoverFill, barFill, whiskerFill,
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
    signals: [{
      name: 'p50Click',
      on: [{
        events: { type: 'click', markname: 'p50' },
        update: '{}',
      }],
    }, {
      name: 'p90Click',
      on: [{
        events: { type: 'click', markname: 'p90' },
        update: '{}',
      }],
    }, {
      name: 'p99Click',
      on: [{
        events: { type: 'click', markname: 'p99' },
        update: '{}',
      }],
    }],
  };
};

const styles = (theme: Theme) => createStyles({
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
    // It seems like ReactVega automatically inserts 5 px padding on the bottom of charts,
    // and there isn't a clear way to turn this off.
    marginTop: 5,
  },
  label: {
    textAlign: 'right',
    justifyContent: 'flex-end',
    width: 100,
    marginLeft: 5,
    marginRight: 10,
  },
  low: {
    color: getColor('low', theme),
  },
  med: {
    color: getColor('med', theme),
  },
  high: {
    color: getColor('high', theme),
  },
});

export type SelectedPercentile = 'p50' | 'p90' | 'p99';

interface QuantilesBoxWhiskerProps extends WithStyles<typeof styles> {
  p50: number;
  p90: number;
  p99: number;
  max: number;
  p50Display: string;
  p90Display: string;
  p99Display: string;
  p50Level: GaugeLevel;
  p90Level: GaugeLevel;
  p99Level: GaugeLevel;
  selectedPercentile: SelectedPercentile;
  // Function to call when the selected percentile is updated.
  onChangePercentile?: (percentile: SelectedPercentile) => void;
}

const QuantilesBoxWhisker = (props: QuantilesBoxWhiskerProps) => {
  const {
    classes, p50, p90, p99, max, p50Level, p90Level, p99Level, p50Display, p90Display, p99Display,
    selectedPercentile, onChangePercentile,
  } = props;
  const theme = useTheme();
  const p50HoverFill = getColor(p50Level, theme);
  const p90HoverFill = getColor(p90Level, theme);
  const p99HoverFill = getColor(p99Level, theme);
  let p50Fill = theme.palette.text.secondary;
  let p90Fill = theme.palette.text.secondary;
  let p99Fill = theme.palette.text.secondary;
  let percentileValue;
  let selectedPercentileDisplay;
  let selectedPercentileLevel;

  const changePercentileIfDifferent = (percentile: SelectedPercentile) => {
    if (percentile !== selectedPercentile) {
      onChangePercentile(percentile);
    }
  };

  switch (selectedPercentile) {
    case 'p50': {
      p50Fill = p50HoverFill;
      percentileValue = p50;
      selectedPercentileDisplay = p50Display;
      selectedPercentileLevel = p50Level;
      break;
    }
    case 'p90': {
      p90Fill = p90HoverFill;
      percentileValue = p90;
      selectedPercentileDisplay = p90Display;
      selectedPercentileLevel = p90Level;
      break;
    }
    case 'p99':
    default: {
      p99Fill = p99HoverFill;
      percentileValue = p99;
      selectedPercentileDisplay = p99Display;
      selectedPercentileLevel = p99Level;
    }
  }

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
      <ReactVega
        className={classes.vegaWrapper}
        signalListeners={{
          p50Click: () => changePercentileIfDifferent('p50'),
          p90Click: () => changePercentileIfDifferent('p90'),
          p99Click: () => changePercentileIfDifferent('p99'),
        }}
        spec={spec}
        actions={false}
        tooltip={tooltipHandler}
      />
      <span className={clsx(classes.label, classes[selectedPercentileLevel])}>
        {selectedPercentileDisplay}
      </span>
    </div>
  );
};

export default withStyles(styles)(QuantilesBoxWhisker);
