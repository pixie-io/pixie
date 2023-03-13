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

import { Theme } from '@mui/material/styles';
import { createStyles, makeStyles } from '@mui/styles';

import { VizierTable } from 'app/api';
import { formatBySemType } from 'app/containers/format-data/format-data';
import { WidgetDisplay } from 'app/containers/live/vis';

export interface StatChartDisplay extends WidgetDisplay {
  readonly title: string;
  readonly stat: {
    readonly value: string;
  };
}

export interface StatChartProps {
  title?: string;
  display: StatChartDisplay;
  table: VizierTable;
  data: any[];
}

const useStyles = makeStyles((theme: Theme) => createStyles({
  root: {
    position: 'relative',
    height: '100%',
    boxSizing: 'content-box', // For the next rules to work properly
    margin: theme.spacing(-0.75), // Push to the edges of the containing box
    paddingBottom: theme.spacing(1.5), // Including the bottom edge
    borderRadius: theme.shape.borderRadius, // And match the shape of that box
    display: 'flex',
    justifyContent: 'center',
    alignItems: 'center',
  },
  title: {
    ...theme.typography.caption,
    marginTop: theme.spacing(0.25),
  },
  label: {
    ...theme.typography.h2,
    textAlign: 'center',
  },
  labelUnits: {
    position: 'relative',
    left: theme.spacing(0.25),
    bottom: '0.75em',
    fontSize: '0.75rem',
  },
}), { name: 'StatChart' });

export const StatChart = React.memo<StatChartProps>(({
  title,
  display,
  table,
  data,
}) => {
  const classes = useStyles();

  const { stat: { value: yAxis }, title: innerTitle } = display;
  const valueST = table.relation.getColumnsList().find(col => col.getColumnName() === yAxis).getColumnSemanticType();
  // Just use the latest value in the data set
  const label = formatBySemType(valueST, data[data.length - 1][yAxis]);

  return (
    <div className={classes.root} data-title={title}>
      <div className={classes.label}>
        {label.val}
        <span className={classes.labelUnits}>{label.units}</span>
        <div className={classes.title}>{innerTitle}</div>
      </div>
    </div>
  );
});
StatChart.displayName = 'StatChart';
