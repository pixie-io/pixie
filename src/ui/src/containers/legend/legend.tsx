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
import { CSSProperties } from 'react';

import {
  KeyboardArrowLeft as KeyboardArrowLeftIcon,
  KeyboardArrowRight as KeyboardArrowRightIcon,
} from '@mui/icons-material';
import { IconButton, Tooltip } from '@mui/material';
import { Theme } from '@mui/material/styles';
import { createStyles, makeStyles } from '@mui/styles';

import { LegendData, LegendEntry } from './legend-data';

const NUM_ROWS = 2;
const MAX_NUM_GRIDS = 4;

const COLOR_COLUMN_SIZE = 8;
const KEY_COLUMN_SIZE = 140;
const VAL_COLUMN_SIZE = 80;
const COLUMN_GAP_SIZE = 5;
const COLUMN_SIZES = `${COLOR_COLUMN_SIZE}px ${KEY_COLUMN_SIZE}px ${VAL_COLUMN_SIZE}px`;
const GRID_WIDTH = (
  COLOR_COLUMN_SIZE + COLUMN_GAP_SIZE
  + KEY_COLUMN_SIZE + COLUMN_GAP_SIZE
  + VAL_COLUMN_SIZE
);
const GRID_GAP_SIZE = 20;

const PAGE_ARROWS_SIZE = 2 * 30;
const calcGridWidth = (numGrids: number) => (numGrids - 1) * GRID_GAP_SIZE + numGrids * GRID_WIDTH + PAGE_ARROWS_SIZE;

const ROW_HEIGHT = 25;
const HEADER_HEIGHT = 25;

// I've provided MIN_WIDTH and MIN_HEIGHT here to be used to prevent the user from making too small a chart.
// But I'm not sure how to do that yet.
export const MIN_HEIGHT = HEADER_HEIGHT + (NUM_ROWS * ROW_HEIGHT);
// NUM_GRIDS is scaled up/down based on width, so the minimum witdth is the width of just 1 grid.
export const MIN_WIDTH = GRID_WIDTH;

export interface LegendInteractState {
  selectedSeries: string[];
  hoveredSeries: string;
}

interface LegendProps {
  data: LegendData;
  vegaOrigin: number[];
  chartWidth: number;
  // eslint-disable-next-line react/no-unused-prop-types
  name: string;
  interactState: LegendInteractState;
  setInteractState: (s: LegendInteractState) => void;
}

const useStyles = makeStyles((theme: Theme) => createStyles({
  gridsContainer: {
    height: '100%',
    width: '100%',
    display: 'flex',
    flexDirection: 'row',
    justifyContent: 'flex-start',
    overflow: 'hidden',
    alignItems: 'center',
    paddingTop: theme.spacing(0.675), // 5px
  },
  rowContainer: {
    display: 'contents',
  },
  colorCircle: {
    height: `${COLOR_COLUMN_SIZE}px`,
    width: `${COLOR_COLUMN_SIZE}px`,
    borderRadius: '50%',
    marginRight: theme.spacing(1),
    display: 'inline-block',
  },
  colorContainer: {
    textAlign: 'center',
  },
  key: {
    ...theme.typography.caption,
    textAlign: 'left',
    textOverflow: 'ellipsis',
    overflow: 'hidden',
    whiteSpace: 'nowrap',
  },
  val: {
    ...theme.typography.caption,
    textAlign: 'right',
  },
  gridGap: {
    height: '100%',
    width: `${GRID_GAP_SIZE}px`,
    minWidth: `${GRID_GAP_SIZE}px`,
  },
  iconContainer: {
    display: 'flex',
    flexDirection: 'row',
    justifyContent: 'center',
    flex: '1',
  },
}), { name: 'Legend' });

const toRowMajorOrder = (entries: LegendEntry[], numCols: number, numRows: number): LegendEntry[] => {
  const newEntries: LegendEntry[] = [];
  for (let i = 0; i < numCols; i++) {
    for (let j = 0; j < numRows; j++) {
      const index = j * numCols + i;
      if (index >= entries.length) {
        newEntries.push(null);
        continue;
      }
      newEntries.push(entries[index]);
    }
  }
  return newEntries;
};

const Legend = React.memo((props: LegendProps) => {
  const classes = useStyles();
  const [currentPage, setCurrentPage] = React.useState<number>(0);
  const {
    interactState, setInteractState, vegaOrigin, chartWidth, data,
  } = props;

  const handleRowLeftClick = React.useCallback((key: string) => {
    // Toggle selected series.
    if (interactState.selectedSeries.includes(key)) {
      setInteractState({
        ...interactState,
        selectedSeries: interactState.selectedSeries.filter((s: string) => s !== key),
      });
    } else {
      setInteractState({
        ...interactState,
        selectedSeries: [...interactState.selectedSeries, key],
      });
      setCurrentPage(0);
    }
  }, [interactState, setInteractState]);

  const handleRowRightClick = React.useCallback((e: React.SyntheticEvent) => {
    // Reset all selected series.
    setInteractState({ ...interactState, selectedSeries: [] });
    // Prevent right click menu from showing up.
    e.preventDefault();
    return false;
  }, [interactState, setInteractState]);

  const handleRowHover = React.useCallback((key: string) => {
    setInteractState({ ...interactState, hoveredSeries: key });
  }, [interactState, setInteractState]);

  const handleRowLeave = React.useCallback(() => {
    setInteractState({ ...interactState, hoveredSeries: null });
  }, [interactState, setInteractState]);

  const handlePageBack = React.useCallback(() => {
    setCurrentPage((page) => page - 1);
  }, []);

  const handlePageForward = React.useCallback(() => {
    setCurrentPage((page) => page + 1);
  }, []);

  if (vegaOrigin.length < 2) {
    return <div />;
  }

  let leftPadding = vegaOrigin[0];
  let rightPadding = 0;

  let numGrids = MAX_NUM_GRIDS;
  // Dynamically take out grids if theres no room for them.
  while ((leftPadding + calcGridWidth(numGrids)) > chartWidth && numGrids > 1) {
    numGrids--;
  }

  // If the legend can't be aligned to the origin of the chart, then center it.
  if (leftPadding + calcGridWidth(numGrids) > chartWidth) {
    const totalPadding = Math.max(0, chartWidth - calcGridWidth(numGrids));
    leftPadding = totalPadding / 2;
    rightPadding = totalPadding / 2;
  }

  let dataEntries: LegendEntry[];
  if (interactState.selectedSeries.length > 0) {
    // Put selected series first.
    dataEntries = data.entries.filter((entry) => interactState.selectedSeries.includes(entry.key));
    dataEntries = [
      ...dataEntries,
      ...data.entries.filter((entry) => !interactState.selectedSeries.includes(entry.key)),
    ];
  } else {
    dataEntries = data.entries;
  }

  const entriesPerPage = numGrids * NUM_ROWS;
  const maxPages = Math.ceil(dataEntries.length / entriesPerPage);
  const pageEntriesStart = currentPage * entriesPerPage;
  const pageEntriesEnd = Math.min((currentPage + 1) * entriesPerPage, dataEntries.length);
  let entries = dataEntries.slice(pageEntriesStart, pageEntriesEnd);
  entries = toRowMajorOrder(entries, numGrids, NUM_ROWS);

  let index = 0;
  // We add a grid per "column" we want to see in the legend.
  // Each grid contains NUM_ROWS rows and 4 columns: (color circle | key | : | value).
  const grids = [];
  for (let i = 0; i < numGrids; i++) {
    const rows = [];
    // We have to break both here and in the inner loop in case we run out of entries before
    // we have reached all of the grids/rows respectively.
    if (index >= entries.length) {
      break;
    }
    for (let j = 0; j < NUM_ROWS; j++) {
      if (index >= entries.length) {
        break;
      }
      const entry = entries[index];
      index++;
      if (!entry) {
        continue;
      }

      const colorStyles: CSSProperties = {
        backgroundColor: entry.color,
      };

      // Handle hover/selection styling.
      const onMouseOver = () => handleRowHover(entry.key);
      const styles: CSSProperties = {
        opacity: '1.0',
      };
      if (interactState.selectedSeries.length > 0 && !interactState.selectedSeries.includes(entry.key)) {
        styles.opacity = '0.3';
      }
      if (interactState.hoveredSeries === entry.key) {
        styles.color = entry.color;
        styles.opacity = '1.0';
      }

      rows.push(
        <div
          key={`row-${j}`}
          className={classes.rowContainer}
          onMouseOver={onMouseOver}
          onMouseOut={handleRowLeave}
          onClick={() => handleRowLeftClick(entry.key)}
        >
          <div style={styles} className={classes.colorContainer}>
            <div className={classes.colorCircle} style={colorStyles} />
          </div>
          <Tooltip title={entry.key} enterDelay={1500}>
            <div style={styles} className={classes.key}>{entry.key === 'undefined' ? 'Unknown' : entry.key}</div>
          </Tooltip>
          <div style={styles} className={classes.val}>{entry.val}</div>
        </div>,
      );
    }

    const gridStyle: CSSProperties = {
      display: 'grid',
      gridTemplateColumns: COLUMN_SIZES,
      columnGap: `${COLUMN_GAP_SIZE}px`,
      gridTemplateRows: `repeat(${NUM_ROWS}, ${ROW_HEIGHT}px)`,
    };
    grids.push(
      <div key={`col-${i}`} style={gridStyle}>
        {rows}
      </div>,
    );
    // If this isn't the last grid, add a spacing div.
    if (i !== numGrids - 1) {
      grids.push(<div className={classes.gridGap} key={`${i}-gap`}/>);
    }
  }

  const containerStyles: CSSProperties = {
    paddingLeft: `${leftPadding}px`,
    paddingRight: `${rightPadding}px`,
    width: '100%',
  };

  return (
    <div
      className={classes.gridsContainer}
      style={containerStyles}
      onContextMenu={handleRowRightClick}
    >
      {grids}
      <div className={classes.iconContainer}>
        <IconButton
          onClick={handlePageBack}
          disabled={currentPage === 0}
          size='small'
        >
          <KeyboardArrowLeftIcon />
        </IconButton>
        <IconButton
          onClick={handlePageForward}
          disabled={currentPage === maxPages - 1}
          size='small'
        >
          <KeyboardArrowRightIcon />
        </IconButton>
      </div>
    </div>
  );
});
Legend.displayName = 'Legend';

export default Legend;
