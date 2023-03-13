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

import {
  Data,
  GroupMark,
  ScaleData,
  Signal,
  SignalRef,
  Spec as VgSpec,
} from 'vega';

import { FormatFnMetadata } from 'app/containers/format-data/format-data';
import { WidgetDisplay } from 'app/containers/live/vis';
import { Relation } from 'app/types/generated/vizierapi_pb';

import {
  addAutosize,
  addAxis,
  addChildWidthHeightSignals,
  addDataSource,
  addGridLayout,
  addGridLayoutMarksForGroupedBars,
  addLabelsToAxes,
  addLegend,
  addMark,
  addScale,
  addSignal,
  addTitle,
  addWidthHeightSignals,
  BASE_SPEC,
  DisplayWithLabels,
  extendDataTransforms,
  extendMarkEncoding,
  getVegaFormatFunc,
  PX_BETWEEN_Y_TICKS,
  TRANSFORMED_DATA_SOURCE_NAME,
  VegaSpecWithProps,
} from './common';


// Padding between bars, specified as fraction of step size.
const BAR_PADDING = 0.5;
const SELECTED_BAR_OPACITY = 0.9;
const UNSELECTED_BAR_OPACITY = 0.2;
const BAR_TEXT_OFFSET = 5;

// Scale object used to hold a stripped down version of
// information necessary for BarChartProps.
interface InternalScale {
  domain: ScaleData | SignalRef;
  type: 'band' | 'linear';
  bins?: SignalRef;
  paddingInner?: number;
}

export interface Bar {
  readonly value: string;
  readonly label: string;
  readonly stackBy?: string;
  readonly groupBy?: string;
  readonly horizontal?: boolean;
}

export interface BarDisplay extends WidgetDisplay, DisplayWithLabels {
  readonly bar: Bar;
}

// Property struct for Vega BarCharts.
export interface BarChartProps {
  barStart: string;
  barEnd?: string;
  groupBy?: string;
  stackBy?: string;
  horizontal: boolean;
  value: string;
  valueFmtFn?: FormatFnMetadata;
  barFmtFn?: FormatFnMetadata;
  transformedDataSrc: Data;
  labelScale: InternalScale;
  display: DisplayWithLabels;
}

function addBarHoverSignal(
  spec: VgSpec,
  label: string,
  barMarkName: string,
  groupTest: string,
  stackTest: string,
): Signal {
  return addSignal(spec, {
    name: 'hovered_bar',
    on: [
      {
        events: [
          {
            source: 'view',
            type: 'mouseover',
            markname: barMarkName,
          },
        ],
        update: `datum && {label: datum["${label}"], group: ${groupTest}, stack: ${stackTest}}`,
      },
      {
        events: [
          {
            source: 'view',
            type: 'mouseout',
            markname: barMarkName,
          },
        ],
        update: 'null',
      },
    ],
  });
}

// The internal function to render barCharts.
export function barChartInternal(chart: BarChartProps, spec: VgSpec): VegaSpecWithProps {
  if (!chart.groupBy) {
    addAutosize(spec);
  }

  let valueField = chart.value;
  let valueStartField = '';
  let valueEndField = valueField;
  if (chart.stackBy) {
    valueField = `sum_${chart.value}`;
    valueStartField = `${valueField}_start`;
    valueEndField = `${valueField}_end`;
    const extraGroupBy = [chart.barStart];
    if (chart.barEnd) {
      extraGroupBy.push(chart.barEnd);
    }
    if (chart.groupBy) {
      extraGroupBy.push(chart.groupBy);
    }
    extendDataTransforms(chart.transformedDataSrc, [
      {
        type: 'aggregate',
        groupby: [chart.stackBy, ...extraGroupBy],
        ops: ['sum'],
        fields: [chart.value],
        as: [valueField],
      },
      {
        type: 'stack',
        groupby: [...extraGroupBy],
        field: valueField,
        sort: { field: [chart.stackBy], order: ['descending'] },
        as: [valueStartField, valueEndField],
        offset: 'zero',
      },
    ]);
  }
  let columnDomainData: Data;
  if (chart.groupBy) {
    columnDomainData = addDataSource(spec, {
      name: 'column-domain',
      source: chart.transformedDataSrc.name,
      transform: [
        {
          type: 'aggregate',
          groupby: [chart.groupBy],
        },
      ],
    });
  }

  const horizontalBars = chart.horizontal;
  // Add signals.
  addWidthHeightSignals(spec);
  const widthName = (chart.groupBy) ? 'child_width' : 'width';
  const heightName = (chart.groupBy) ? 'child_height' : 'height';
  if (chart.groupBy) {
    const groupScale = addScale(spec, {
      name: 'group-scale',
      type: 'band',
      domain: {
        data: chart.transformedDataSrc.name,
        field: chart.groupBy,
        sort: true,
      },
      range: (horizontalBars) ? 'height' : 'width',
    });
    addChildWidthHeightSignals(spec, widthName, heightName, horizontalBars, groupScale.name);
  }
  const barMarkName = 'bar-mark';
  const groupTest = (datum: string) => ((chart.groupBy) ? `${datum}["${chart.groupBy}"]` : 'null');
  const stackTest = (datum: string) => ((chart.stackBy) ? `${datum}["${chart.stackBy}"]` : 'null');
  const hoverSignal = addBarHoverSignal(spec, chart.barStart, barMarkName, groupTest('datum'), stackTest('datum'));
  const isHovered = (datum: string) => `${hoverSignal.name}.label === ${datum}["${chart.barStart}"] && `
    + `${hoverSignal.name}.group === ${groupTest(datum)} && `
    + `${hoverSignal.name}.stack === ${stackTest(datum)}`;

  // Add scales.
  const labelScale = addScale(spec, {
    name: (horizontalBars) ? 'y' : 'x',
    range: (horizontalBars) ? [{ signal: heightName }, 0] : [0, { signal: widthName }],
    ...chart.labelScale,
  });

  const valueScale = addScale(spec, {
    name: (horizontalBars) ? 'x' : 'y',
    type: 'linear',
    domain: {
      data: chart.transformedDataSrc.name,
      fields: (valueStartField) ? [valueStartField, valueEndField] : [valueField],
    },
    range: (horizontalBars) ? [0, { signal: widthName }] : [{ signal: heightName }, 0],
    nice: true,
    zero: true,
  });

  const colorScale = addScale(spec, {
    name: 'color',
    type: 'ordinal',
    range: 'category',
    domain: (!chart.stackBy) ? [valueField] : {
      data: chart.transformedDataSrc.name,
      field: chart.stackBy,
      sort: true,
    },
  });

  // Add marks.
  let group: VgSpec | GroupMark = spec;
  let dataName = chart.transformedDataSrc.name;
  let groupForValueAxis: VgSpec | GroupMark = spec;
  let groupForLabelAxis: VgSpec | GroupMark = spec;
  if (chart.groupBy) {
    // We use vega's grid layout functionality to plot grouped bars.
    ({ groupForValueAxis, groupForLabelAxis } = addGridLayoutMarksForGroupedBars(
      spec, chart.groupBy, chart.barStart, columnDomainData, horizontalBars, widthName, heightName));
    addGridLayout(spec, columnDomainData, horizontalBars);
    dataName = 'facetedData';
    group = addMark(spec, {
      name: 'barGroup',
      type: 'group',
      style: 'cell',
      from: {
        facet: {
          name: dataName,
          data: chart.transformedDataSrc.name,
          groupby: [chart.groupBy],
        },
      },
      sort: {
        field: [`datum["${chart.groupBy}"]`],
        order: ['ascending'],
      },
      encode: {
        update: {
          width: {
            signal: widthName,
          },
          height: {
            signal: heightName,
          },
        },
      },
      axes: [
        {
          scale: valueScale.name,
          orient: (horizontalBars) ? 'bottom' : 'left',
          grid: true,
          gridScale: labelScale.name,
          tickCount: { signal: `ceil(${heightName}/${PX_BETWEEN_Y_TICKS})` },
          labelOverlap: true,
          labels: false,
          ticks: false,
        },
      ],
    }) as GroupMark;
  }

  const barMark = addMark(group, {
    name: barMarkName,
    type: 'rect',
    style: 'bar',
    from: {
      data: dataName,
    },
    encode: {
      update: {
        fill: {
          scale: colorScale.name,
          ...((chart.stackBy) ? { field: chart.stackBy } : { value: valueField }),
        },
        opacity: [
          { test: `!${hoverSignal.name}`, value: SELECTED_BAR_OPACITY },
          {
            test: `${hoverSignal.name} && datum && ${isHovered('datum')}`,
            value: SELECTED_BAR_OPACITY,
          },
          { value: UNSELECTED_BAR_OPACITY },
        ],
      },
    },
  });

  let barWidthEncodingKey = (horizontalBars) ? 'height' : 'width';
  let barWidthEncodingValue: any = { scale: labelScale.name, band: 1 };
  if (chart.barEnd) {
    barWidthEncodingKey = (horizontalBars) ? 'y2' : 'x2';
    barWidthEncodingValue = { scale: labelScale.name, field: chart.barEnd };
  }

  extendMarkEncoding(barMark, 'update', {
    // Bottom of the bar. If there's no "startField", then just 0.
    [(horizontalBars) ? 'x' : 'y']: {
      scale: valueScale.name,
      ...((valueStartField) ? { field: valueStartField } : { value: 0 }),
    },
    // Top of the bar.
    [(horizontalBars) ? 'x2' : 'y2']: {
      scale: valueScale.name,
      field: valueEndField,
    },
    // The label of the bar.
    [(horizontalBars) ? 'y' : 'x']: {
      scale: labelScale.name,
      field: chart.barStart,
    },
    // The width of the bar.
    [barWidthEncodingKey]: barWidthEncodingValue,
  });

  addMark(group, {
    name: 'bar-value-text',
    type: 'text',
    style: 'bar-value-text',
    from: {
      data: barMarkName,
    },
    encode: {
      enter: {
        text: { field: `datum["${valueField}"]` },
        ...((horizontalBars)
          ? {
            x: { field: 'x2', offset: BAR_TEXT_OFFSET },
            y: { field: 'y', offset: { field: 'height', mult: 0.5 } },
            baseline: { value: 'middle' },
            align: { value: 'left' },
          }
          : {
            x: { field: 'x', offset: { field: 'width', mult: 0.5 } },
            y: { field: 'y', offset: -BAR_TEXT_OFFSET },
            baseline: { value: 'bottom' },
            align: { value: 'center' },
          }),
      },
      update: {
        opacity: [
          { test: `${hoverSignal.name} && datum && datum.datum && ${isHovered('datum.datum')}`, value: 1.0 },
          { value: 0.0 },
        ],
      },
    },
  });

  const xHasGrid = !chart.groupBy && !!horizontalBars;
  const yHasGrid = !chart.groupBy && !horizontalBars;

  let xFormatAxis = {};
  let yFormatAxis = {};

  const applyMetadataFnToAxis = (name) => ({
    encode: {
      labels: { update: { text: { signal: `${name}(datum.value)` } } },
    },
  });
  if (chart.valueFmtFn) {
    if (horizontalBars) {
      xFormatAxis = applyMetadataFnToAxis(chart.valueFmtFn.name);
    } else {
      yFormatAxis = applyMetadataFnToAxis(chart.valueFmtFn.name);
    }
  }
  if (chart.barFmtFn) {
    if (horizontalBars) {
      yFormatAxis = applyMetadataFnToAxis(chart.barFmtFn.name);
    } else {
      xFormatAxis = applyMetadataFnToAxis(chart.barFmtFn.name);
    }
  }
  const xAxis = addAxis((horizontalBars) ? groupForValueAxis : groupForLabelAxis, {
    scale: (horizontalBars) ? valueScale.name : labelScale.name,
    orient: 'bottom',
    grid: xHasGrid,
    gridScale: (xHasGrid) ? labelScale.name : null,
    labelAlign: (horizontalBars) ? 'center' : 'right',
    labelAngle: (horizontalBars) ? 0 : 270,
    labelBaseline: 'middle',
    labelOverlap: true,
    labelSeparation: 3,
    // Tick count is only used if this is the valueAxis.
    tickCount: {
      signal: `ceil(${widthName}/${PX_BETWEEN_Y_TICKS})`,
    },
    ...xFormatAxis,
  });
  const yAxis = addAxis((horizontalBars) ? groupForLabelAxis : groupForValueAxis, {
    scale: (horizontalBars) ? labelScale.name : valueScale.name,
    orient: 'left',
    grid: yHasGrid,
    gridScale: yHasGrid ? labelScale.name : null,
    labelOverlap: true,
    // Tick count is only used if this is the valueAxis.
    tickCount: {
      signal: `ceil(${heightName}/${PX_BETWEEN_Y_TICKS})`,
    },
    ...yFormatAxis,
  });
  addLabelsToAxes(xAxis, yAxis, chart.display);

  if (chart.stackBy) {
    addLegend(spec, {
      fill: colorScale.name,
      symbolType: 'square',
      title: chart.stackBy,
      encode: {
        symbols: {
          update: {
            stroke: {
              value: null,
            },
          },
        },
      },
    });
  }

  if (chart.display.title) {
    addTitle(spec, chart.display.title);
  }

  return {
    spec,
    hasLegend: false,
    legendColumnName: '',
  };
}

export function convertToBarChart(display: BarDisplay, source: string, relation?: Relation): VegaSpecWithProps {
  if (!display.bar) {
    throw new Error('BarChart must have an entry for property bar');
  }
  if (!display.bar.value) {
    throw new Error('BarChart property bar must have an entry for property value');
  }
  if (!display.bar.label) {
    throw new Error('BarChart property bar must have an entry for property label');
  }

  const spec = { ...BASE_SPEC, style: 'cell' };
  // Add data and transforms.
  const baseDataSrc = addDataSource(spec, { name: source });
  const transformedDataSrc = addDataSource(spec, {
    name: TRANSFORMED_DATA_SOURCE_NAME, source: baseDataSrc.name, transform: [],
  });
  // Horizontal should be default.
  const horizontalBars = (display.bar.horizontal === undefined) ? true : display.bar.horizontal;

  const formatFuncMD = getVegaFormatFunc(relation, display.bar.value);
  return barChartInternal({
    barStart: display.bar.label,
    horizontal: horizontalBars,
    value: display.bar.value,
    valueFmtFn: formatFuncMD,
    transformedDataSrc,
    groupBy: display.bar.groupBy,
    stackBy: display.bar.stackBy,
    labelScale: {
      domain: {
        data: transformedDataSrc.name,
        field: display.bar.label,
        sort: true,
      },
      paddingInner: BAR_PADDING,
      type: 'band',
    },
    display,
  }, spec);
}
