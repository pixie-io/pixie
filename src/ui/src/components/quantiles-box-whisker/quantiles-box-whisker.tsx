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

import { Tooltip } from '@mui/material';
import { Theme, useTheme } from '@mui/material/styles';
import { createStyles, makeStyles } from '@mui/styles';

const useStyles = makeStyles((theme: Theme) => createStyles({
  root: {
    display: 'flex',
    alignItems: 'center',
    flexGrow: 1,
  },
  chart: {
    margin: 0, // <figure> has a margin by default.
    flex: '1 1 100%',
    overflowX: 'hidden',
    textAlign: 'left',
    position: 'relative',
    height: theme.spacing(1.75), // 14px
    cursor: 'default',

    '& > *': {
      position: 'absolute',
      top: 0,
    },
  },
  greenBar: {
    left: 0,
    height: '100%',
    backgroundColor: theme.palette.primary.dark,
    zIndex: 1, // Appear above the whisker
    opacity: 0.7,
  },
  pBar: {
    height: '100%',
    width: theme.spacing(1),
    transform: 'translateX(-4px)',
    cursor: 'pointer',
    opacity: 0.9,
    zIndex: 2, // Make sure the clickable area is above the green bar
    // It's a button (for both accessibility and to separate click events properly), so remove the default styles too
    border: 'none',
    margin: 0,
    padding: 0,
    background: 'transparent',
    '&::after': {
      display: 'block',
      position: 'relative',
      top: 0,
      left: theme.spacing(0.375),
      content: '""',
      width: theme.spacing(0.25),
      backgroundColor: 'var(--fill)',
      height: '100%',
    },
    '&:hover': {
      '&::after': {
        backgroundColor: 'var(--hover-fill)',
      },
    },
  },
  whisker: {
    top: '49%',
    height: '1px',
    width: '100%',
    backgroundColor: theme.palette.text.primary,
    zIndex: 0, // Under both the green bar and the p50/p90/p99 bars
  },
  label: {
    textAlign: 'right',
    justifyContent: 'flex-end',
    width: theme.spacing(8), // 64px
    flex: '0 0 auto',
    marginLeft: 5,
    marginRight: 10,
  },
}), { name: 'QuantilesBoxWhisker' });

export type SelectedPercentile = 'p50' | 'p90' | 'p99';

const PercentileBar: React.FC<{
  onClick: () => void;
  fill: string;
  hoverFill: string;
  tooltip: string;
  left: string;
}> = React.memo(({
  onClick, fill, hoverFill, tooltip, left,
}) => {
  const classes = useStyles();
  const style = {
    left,
    '--fill': fill,
    '--hover-fill': hoverFill,
  } as React.CSSProperties;
  return (
    // eslint-disable-next-line react-memo/require-usememo
    <Tooltip title={tooltip} enterDelay={0} enterNextDelay={0} TransitionProps={{ timeout: 0 }}>
      <button
        aria-label={tooltip}
        className={classes.pBar}
        style={style}
        onClick={onClick}
      />
    </Tooltip>
  );
});
PercentileBar.displayName = 'PercentileBar';

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

export const QuantilesBoxWhisker: React.FC<QuantilesBoxWhiskerProps> = React.memo(({
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
}) => {
  /* eslint-disable react-memo/require-usememo */
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

  // Translate values to a percentage of the highest value among all rows; use a nonlinear scale so that microsecond
  // response times don't end up 1px wide when there was a single 1sec outlier.
  const rtMax = Math.sqrt(max);
  const [p50pct, p90pct, p99pct] = [p50, p90, p99].map((v) => `${99 * Math.sqrt(v) / rtMax}%`);

  const a11yLabel = `p50: ${p50Display}; p90: ${p90Display}; p99: ${p99Display}`;

  return (
    <div className={classes.root} role='graphics-document' aria-roledescription='visualization' aria-label={a11yLabel}>
      {/* Using <figure> so that a click handler in data-table.tsx can decide whether to ignore the click event. */}
      <figure className={classes.chart}>
        <div className={classes.greenBar} style={{ width: p90pct }} />
        <div className={classes.whisker} style={{ left: p90pct, width: `calc(${p99pct} - ${p90pct})` }} />
        <PercentileBar
          onClick={() => changePercentileIfDifferent('p50')}
          fill={p50Fill}
          hoverFill={p50HoverFill}
          tooltip={`p50: ${p50Display}`}
          left={p50pct} />
        <PercentileBar
          onClick={() => changePercentileIfDifferent('p90')}
          fill={p90Fill}
          hoverFill={p90HoverFill}
          tooltip={`p90: ${p90Display}`}
          left={p90pct} />
        <PercentileBar
          onClick={() => changePercentileIfDifferent('p99')}
          fill={p99Fill}
          hoverFill={p99HoverFill}
          tooltip={`p99: ${p99Display}`}
          left={p99pct} />
      </figure>
      <span className={classes.label} style={{ color: selectedPercentileFill }}>
        {selectedPercentileDisplay}
      </span>
    </div>
  );
  /* eslint-enable react-memo/require-usememo */
});
QuantilesBoxWhisker.displayName = 'QuantilesBoxWhisker';
