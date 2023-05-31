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

/* eslint-disable no-underscore-dangle */

import * as React from 'react';

import { Theme, useTheme } from '@mui/material/styles';
import { createStyles, makeStyles } from '@mui/styles';
import { useFlags } from 'launchdarkly-react-client-sdk';
import { Vega as ReactVega } from 'react-vega';
import { View } from 'vega-typings';

import Legend, { LegendInteractState } from 'app/containers/legend/legend';
import {
  buildHoverDataCache, formatLegendData, HoverDataCache, LegendData,
} from 'app/containers/legend/legend-data';
import {
  EXTERNAL_HOVER_SIGNAL,
  getVegaFormatFunc,
  HOVER_PIVOT_TRANSFORM,
  HOVER_SIGNAL,
  INTERNAL_HOVER_SIGNAL,
  LEGEND_HOVER_SIGNAL,
  LEGEND_SELECT_SIGNAL,
  REVERSE_HOVER_SIGNAL,
  REVERSE_SELECT_SIGNAL,
  REVERSE_UNSELECT_SIGNAL,
} from 'app/containers/live/convert-to-vega-spec/common';
import {
  ChartDisplay,
  convertWidgetDisplayToVegaSpec,
  getColumnFromDisplay,
  wrapFormatFn,
} from 'app/containers/live/convert-to-vega-spec/convert-to-vega-spec';
import { SHIFT_CLICK_FLAMEGRAPH_SIGNAL } from 'app/containers/live/convert-to-vega-spec/flamegraph';
import {
  EXTERNAL_TS_DOMAIN_SIGNAL,
  INTERNAL_TS_DOMAIN_SIGNAL,
} from 'app/containers/live/convert-to-vega-spec/timeseries';
import {
  Relation, SemanticType,
} from 'app/types/generated/vizierapi_pb';
import { formatFloat64Data } from 'app/utils/format-data';
import noop from 'app/utils/noop';

import { FlamegraphIDEMenu } from './flamegraph-ide';
import { TimeSeriesContext } from '../context/time-series-context';

const NUMERAL_FORMAT_STRING = '0.00';

const useStyles = makeStyles((theme: Theme) => createStyles({
  flexbox: {
    height: '100%',
    width: '100%',
    display: 'flex',
    flexDirection: 'column',
  },
  reactVega: {
    flex: 1,
    minHeight: 0,
  },
  legends: {
    height: '15%',
  },
  noDataMsgContainer: {
    height: '100%',
    display: 'flex',
    justifyContent: 'center',
    justifyItems: 'center',
    alignItems: 'center',
  },
  noDataMessage: {
    ...theme.typography.body1,
  },
}), { name: 'Vega' });

interface VegaProps {
  data: Array<Record<string, unknown>>;
  relation: Relation;
  tableName: string;
  display: ChartDisplay;
  // eslint-disable-next-line react/require-default-props
  className?: string;
}

function getFormatter(relation: Relation, display: ChartDisplay) {
  const column = getColumnFromDisplay(display);
  if (column) {
    const formatFunc = getVegaFormatFunc(relation, column);
    if (formatFunc) {
      return wrapFormatFn(formatFunc.formatFn);
    }
  }
  return (val: number): string => formatFloat64Data(val, NUMERAL_FORMAT_STRING);
}

function getCleanerForSemanticType(semType: SemanticType) {
  switch (semType) {
    case SemanticType.ST_SERVICE_NAME:
      return (val) => (val || 'no linked service');
    case SemanticType.ST_POD_NAME:
      return (val) => (val || 'no linked pod');
    default:
      return null;
  }
}

function cleanInputData(relation: Relation, data: Array<Record<string, unknown>>) {
  const columnToCleaner = Object.fromEntries(relation.getColumnsList().map((colInfo) => {
    const colName = colInfo.getColumnName();
    const colST = colInfo.getColumnSemanticType();
    return [colName, getCleanerForSemanticType(colST)];
  }));
  return data.map((row: any) => {
    return Object.fromEntries(
      Object.entries(row).map(([key, val]) => {
        if (columnToCleaner[key]) {
          return [key, columnToCleaner[key](val)];
        }
        return [key, val];
      }),
    );
  });
}

const Vega = React.memo((props: VegaProps) => {
  const classes = useStyles();
  const theme = useTheme();
  const {
    data: inputData, tableName, display, relation,
  } = props;
  const {
    spec, hasLegend, legendColumnName, error, preprocess, showTooltips,
  } = React.useMemo(() => convertWidgetDisplayToVegaSpec(display, tableName, theme, relation),
    [display, tableName, theme, relation]);

  const { showInIde } = useFlags();

  const formatter = React.useMemo(() => getFormatter(relation, display), [display, relation]);

  const cleanedInputData = React.useMemo(() => cleanInputData(relation, inputData), [relation, inputData]);

  const data = React.useMemo(() => {
    if (preprocess == null) {
      return { [tableName]: cleanedInputData };
    }

    return { [tableName]: preprocess(cleanedInputData) };
  }, [preprocess, tableName, cleanedInputData]);

  const {
    setHoverTime,
    setTimeseriesDomain: setTSDomain,
    hoverTime: externalHoverTime,
    timeseriesDomain: externalTSDomain,
  } = React.useContext(TimeSeriesContext);

  const [currentView, setCurrentView] = React.useState<View>(null);
  const [vegaOrigin, setVegaOrigin] = React.useState<number[]>([]);
  const [legendData, setLegendData] = React.useState<LegendData>({ time: '', entries: [] });
  const [legendInteractState, setLegendInteractState] = React.useState<LegendInteractState>(
    { selectedSeries: [], hoveredSeries: '' },
  );
  const [hoverDataCache, setHoverDataCache] = React.useState<HoverDataCache>(null);
  const [flamegraphMenuOpen, setFlamegraphMenuOpen] = React.useState(false);
  const [flamegraphSymbol, setFlamegraphSymbol] = React.useState<string>('');
  const [flamegraphPath, setFlamegraphPath] = React.useState<string>('');

  const fgMenuClose = React.useCallback(() => setFlamegraphMenuOpen(false), [setFlamegraphMenuOpen]);

  const chartRef = React.useRef(null);

  const widthListener = React.useCallback(() => {
    if (!currentView) {
      return;
    }
    setVegaOrigin(currentView.origin());
  }, [currentView]);

  const hoverListener = React.useCallback((name, value) => {
    if (!currentView || !hoverDataCache) {
      return;
    }
    if (value && value.time_) {
      const unformattedEntries = hoverDataCache.timeHashMap[value.time_];
      if (unformattedEntries) {
        setLegendData(formatLegendData(currentView, value.time_, unformattedEntries, formatter));
      }
    }
  }, [currentView, hoverDataCache, formatter]);

  const updateView = React.useMemo(() => {
    let called = false;
    return () => {
      if (called) {
        return;
      }
      called = true;
      setTimeout(() => {
        currentView.runAsync().then();
        called = false;
      });
    };
  }, [currentView]);

  const signalListeners = React.useMemo(() => ({
    // Add listener for internal tooltip signal.
    // This internal signal is null, unless the chart is active, so this listener only updates the global
    // hover time context, if this is the active chart.
    [INTERNAL_HOVER_SIGNAL]: (name, value) => {
      if (value && value.time_) {
        setHoverTime(value.time_);
      }
    },

    [INTERNAL_TS_DOMAIN_SIGNAL]: (name, value) => {
      if (!value || value.length !== 2) {
        return;
      }
      const newDomain: [number, number] = [value[0], value[1]];

      setTSDomain((oldDomain) => {
        if (!oldDomain || oldDomain.length !== 2 || !oldDomain[0] || !oldDomain[1]) {
          return newDomain;
        }
        const merged: [number, number] = [Math.min(oldDomain[0], newDomain[0]), Math.max(oldDomain[1], newDomain[1])];
        if (merged[0] === oldDomain[0] && merged[1] === oldDomain[1]) {
          return oldDomain;
        }
        return merged;
      });
    },

    [SHIFT_CLICK_FLAMEGRAPH_SIGNAL]: (name, value) => {
      setFlamegraphSymbol(value.symbol);
      setFlamegraphPath(value.path);
      setFlamegraphMenuOpen(true);
    },

    // Add signal listener for width, because the origin changes when width changes.
    width: widthListener,
    // Add signal listener for the merged hover signal. This listener updates the values in the legend.
    [HOVER_SIGNAL]: hoverListener,
    [REVERSE_HOVER_SIGNAL]: (name, value) => {
      if (!value) {
        setLegendInteractState((state) => ({ ...state, hoveredSeries: '' }));
        return;
      }
      setLegendInteractState((state) => ({
        ...state,
        hoveredSeries: value,
      }));
    },
    [REVERSE_SELECT_SIGNAL]: (_name, value) => {
      if (!value) {
        return;
      }
      setLegendInteractState((state) => {
        if (state.selectedSeries.includes(value)) {
          return { ...state, selectedSeries: state.selectedSeries.filter((s) => s !== value) };
        }
        return { ...state, selectedSeries: [...state.selectedSeries, value] };
      });
    },
    [REVERSE_UNSELECT_SIGNAL]: (name, value) => {
      if (!value) {
        return;
      }
      setLegendInteractState((state) => ({ ...state, selectedSeries: [] }));
    },
  }), [hoverListener, setHoverTime, setLegendInteractState, setTSDomain, widthListener]);

  const onNewView = React.useCallback((view: View) => {
    // Disable default tooltip handling in vega.
    if (!showTooltips) {
      view.tooltip(noop);
    }

    // Add listener for changes to the legend data. If the data changes we rebuild our hash map cache of the data.
    view.addDataListener(HOVER_PIVOT_TRANSFORM, (name, value) => {
      if (value) {
        const cache = buildHoverDataCache(value);
        if (cache) {
          setHoverDataCache(cache);
        }
      }
    });
    // Width listener only kicks in on width changes, so we have to update on new view as well.
    setVegaOrigin(view.origin());
    setCurrentView(view);
  }, [showTooltips]);

  // This effect sets the hover initial state to be the last time value.
  React.useEffect(() => {
    if (currentView && hoverDataCache && hoverDataCache.maxTime) {
      setHoverTime(hoverDataCache.maxTime);
      // set the legend data since the signal listener might not be added yet when we set the hover signal.
      const unformattedEntries = hoverDataCache.timeHashMap[hoverDataCache.maxTime];
      setLegendData(formatLegendData(currentView, hoverDataCache.maxTime, unformattedEntries, formatter));
    }
  }, [hoverDataCache, currentView, setHoverTime, formatter]);

  React.useEffect(() => {
    if (!currentView) {
      return;
    }
    if (externalTSDomain && externalTSDomain.length === 2
      && !Number.isNaN(externalTSDomain[0]) && !Number.isNaN(externalTSDomain[1])) {
      currentView.signal(EXTERNAL_TS_DOMAIN_SIGNAL, externalTSDomain);
    }
    updateView();
  }, [externalTSDomain, currentView, updateView]);

  // Inject the selected series into the corresponding vega signal for this chart.
  React.useEffect(() => {
    if (!currentView || !externalHoverTime || !hoverDataCache) {
      return;
    }
    let time = externalHoverTime;
    // Handle cases where externalHoverTime is not contained within the limits of this chart.
    if (externalHoverTime < hoverDataCache.minTime) {
      time = hoverDataCache.minTime;
    } else if (externalHoverTime > hoverDataCache.maxTime) {
      time = hoverDataCache.maxTime;
    }
    currentView.signal(EXTERNAL_HOVER_SIGNAL, { time_: time });
    updateView();
  }, [externalHoverTime, currentView, hoverDataCache, updateView]);

  React.useEffect(() => {
    if (!currentView) {
      return;
    }
    currentView.signal(LEGEND_HOVER_SIGNAL, legendInteractState.hoveredSeries);
    currentView.signal(LEGEND_SELECT_SIGNAL, legendInteractState.selectedSeries);
    updateView();
  }, [currentView, legendInteractState.hoveredSeries, updateView, legendInteractState.selectedSeries]);

  if (inputData.length === 0) {
    return (
      <div className={props.className}>
        <div className={classes.noDataMsgContainer}>
          <span className={classes.noDataMessage}>{`No data available for ${tableName} table`}</span>
        </div>
      </div>
    );
  }

  return (
    <div className={props.className}>
      {error ? <div>{error.toString()}</div>
        : (
          <div className={classes.flexbox} ref={chartRef}>
            { showInIde ? <FlamegraphIDEMenu
              symbol={flamegraphSymbol}
              fullPath={flamegraphPath}
              open={flamegraphMenuOpen}
              onClose={fgMenuClose}
            /> : null }
            <ReactVega
              spec={spec}
              data={data}
              actions={false}
              onNewView={onNewView}
              className={classes.reactVega}
              signalListeners={signalListeners}
            />
            {!hasLegend ? null
              : (
                <div className={classes.legends}>
                  <Legend
                    data={legendData}
                    vegaOrigin={vegaOrigin}
                    chartWidth={chartRef.current ? chartRef.current.getBoundingClientRect().width : 0}
                    name={legendColumnName}
                    interactState={legendInteractState}
                    setInteractState={setLegendInteractState}
                  />
                </div>
              )}
          </div>
        )}
    </div>
  );
});
Vega.displayName = 'Vega';

export default Vega;
