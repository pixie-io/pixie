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

import 'react-grid-layout/css/styles.css';
import 'react-resizable/css/styles.css';

import * as React from 'react';

import { Alert, AlertTitle, Paper } from '@mui/material';
import {
  alpha, Theme,
} from '@mui/material/styles';
import { createStyles, makeStyles } from '@mui/styles';
import * as GridLayout from 'react-grid-layout';

import { VizierQueryError, VizierTable } from 'app/api';
import { isPixieEmbedded } from 'app/common/embed-context';
import { VizierErrorDetails } from 'app/common/errors';
import { buildClass, Spinner } from 'app/components';
import { LiveRouteContext } from 'app/containers/App/live-routing';
import { StatChart, StatChartDisplay } from 'app/containers/live-widgets/charts/stat-chart';
import { TextChart, TextChartDisplay } from 'app/containers/live-widgets/charts/text-chart';
import {
  TimeSeriesContext, withTimeSeriesContext,
} from 'app/containers/live-widgets/context/time-series-context';
import { GraphDisplay, GraphWidget } from 'app/containers/live-widgets/graph/graph';
import { RequestGraphWidget } from 'app/containers/live-widgets/graph/request-graph';
import { RequestGraphDisplay } from 'app/containers/live-widgets/graph/request-graph-manager';
import { QueryResultTableDisplay, QueryResultTable } from 'app/containers/live-widgets/table/query-result-viewer';
import Vega from 'app/containers/live-widgets/vega/vega';
import { LayoutContext } from 'app/context/layout-context';
import { ResultsContext } from 'app/context/results-context';
import { ScriptContext } from 'app/context/script-context';
import { containsMutation } from 'app/utils/pxl';
import { triggerResize } from 'app/utils/resize';
import { dataFromProto } from 'app/utils/result-data-utils';

import {
  addLayout, addTableLayout, getGridWidth, Layout, toLayout, updatePositions,
} from './layout';
import MutationModal from './mutation-modal';
import {
  DISPLAY_TYPE_KEY, GRAPH_DISPLAY_TYPE, REQUEST_GRAPH_DISPLAY_TYPE,
  TABLE_DISPLAY_TYPE, Vis, widgetTableName, WidgetDisplay as VisWidgetDisplay,
  STAT_CHART_DISPLAY_TYPE, TEXT_CHART_DISPLAY_TYPE,
} from './vis';

const useStyles = makeStyles((theme: Theme) => createStyles({
  centerContent: {
    width: '100%',
    height: '100%',
    display: 'flex',
    alignItems: 'center',
    justifyContent: 'center',
  },
  grid: {
    '& .react-grid-item.react-grid-placeholder': {
      borderRadius: theme.spacing(0.5),
    },
  },
  gridItem: {
    padding: theme.spacing(0.8),
    borderRadius: '5px',
    display: 'flex',
    flexDirection: 'column',
    alignItems: 'stretch',
  },
  editable: {
    border: theme.palette.border.unFocused,
    boxShadow: theme.shadows[10],
    borderColor: alpha(theme.palette.primary.dark, 0.50),
    cursor: 'move',
    '& > *': {
      pointerEvents: 'none',
    },
    '& .react-resizable-handle': {
      pointerEvents: 'all',
      '&::after': {
        borderColor: theme.palette.foreground.one,
        width: theme.spacing(1),
        height: theme.spacing(1),
      },
    },
  },
  widgetTitlebar: {
    backgroundColor: theme.palette.background.four,
    color: theme.palette.text.primary,
    height: theme.spacing(6),
    padding: theme.spacing(1.5),
    margin: theme.spacing(-0.75),
    marginBottom: 0,
    borderTopLeftRadius: 'inherit',
    borderTopRightRadius: 'inherit',
    borderBottom: `1px ${theme.palette.background.two} solid`,
    display: 'flex',
    flexFlow: 'row nowrap',
    justifyContent: 'space-between',
    alignItems: 'center',
  },
  widgetTitle: {
    flex: '0 0 auto',
    fontSize: theme.spacing(2),
    fontWeight: 500,
    textTransform: 'capitalize',
  },
  widgetInfix: {
    flex: '1 1 auto',
  },
  widgetAffix: {
    flex: '0 0 auto',
  },
  chart: {
    flex: 1,
    minHeight: 0,
    maxWidth: '100%',
  },
  loading: {
    opacity: 0.6,
    pointerEvents: 'none',
  },
  spinner: {
    position: 'absolute',
    top: '50%',
    left: '50%',
    transform: 'translate(-50%, -50%)',
  },
  errorDisplay: {
    margin: `${theme.spacing(2.5)} ${theme.spacing(2.5)} 0`,
  },
  graphContainer: {
    display: 'flex',
    flexDirection: 'column',
    height: '100%',
  },
  vegaWidget: {
    // Bust out of the extra padding from the <Paper/> container
    width: `calc(100% + ${theme.spacing(1.5)})`,
    margin: theme.spacing(-0.75),
    marginTop: 0,
    borderBottomLeftRadius: 'inherit',
    borderBottomRightRadius: 'inherit',

    flex: 1,
    display: 'flex',
    border: '1px solid transparent',
    '&.focus': {
      border: `1px solid ${theme.palette.foreground.grey2}`,
    },
    minHeight: 0,
  },
}), { name: 'Canvas' });

interface VegaWidgetProps {
  tableName: string;
  display: VisWidgetDisplay;
  table: VizierTable;
  data: any[];
}

const VegaWidget: React.FC<VegaWidgetProps> = React.memo(({
  tableName,
  display,
  table,
  data,
}) => {
  const classes = useStyles();
  const [focused, setFocused] = React.useState(false);
  const toggleFocus = React.useCallback(() => setFocused((enabled) => !enabled), []);

  // Workaround: Vega consumes the focus event from a click, but not the click itself.
  const root = React.useRef<HTMLDivElement>(null);
  const takeFocus = React.useCallback(() => root.current?.focus(), []);

  const className = buildClass(classes.vegaWidget, focused && 'focus');

  return (
    <div ref={root} className={className} onFocus={toggleFocus} onBlur={toggleFocus} onClick={takeFocus} tabIndex={0}>
      <Vega
        className={classes.chart}
        data={data}
        display={display as React.ComponentProps<typeof Vega>['display']}
        relation={table.relation}
        tableName={tableName}
      />
    </div>
  );
});
VegaWidget.displayName = 'VegaWidget';

const WidgetTitlebar = React.memo<{
  title: string,
  affix?: React.ReactNode,
}>(({ title, affix }) => {
  const classes = useStyles();
  return (
    <div className={classes.widgetTitlebar}>
      <div className={classes.widgetTitle}>{title}</div>
      <div className={classes.widgetInfix}>{' '}</div>
      <div className={classes.widgetAffix}>{affix}</div>
    </div>
  );
});
WidgetTitlebar.displayName = 'WidgetTitlebar';

const WidgetDisplay: React.FC<{
  display: VisWidgetDisplay,
  table: VizierTable,
  tableName: string,
  widgetName: string,
  propagatedArgs: Record<string, any>,
  emptyTableMsg: string,
}> = React.memo(({
  display, table, tableName, widgetName, propagatedArgs, emptyTableMsg,
}) => {
  const classes = useStyles();
  const [affix, setAffix] = React.useState<React.ReactNode>(null);
  const affixRef = React.useCallback((el: React.ReactNode) => { setAffix(el); }, []);

  // Needs to be memoized to avoid making graphs re-render every single frame
  const parsedTable = React.useMemo(
    () => table ? dataFromProto(table.relation, table.batches) : [],
    // eslint-disable-next-line react-hooks/exhaustive-deps
    [table?.relation, table?.batches],
  );

  // Before we do any other processing, check if we have the chart that exist entirely in the vis spec with no data
  if (display[DISPLAY_TYPE_KEY] === TEXT_CHART_DISPLAY_TYPE) {
    return (
      <>
        <WidgetTitlebar title={widgetName} />
        <TextChart display={display as TextChartDisplay} />
      </>
    );
  }

  if (!table) {
    const msg = emptyTableMsg || `"${tableName}" not found`;
    return (
      <div>
        {msg}
      </div>
    );
  }

  if (display[DISPLAY_TYPE_KEY] === TABLE_DISPLAY_TYPE) {
    return (
      <>
        <WidgetTitlebar title={widgetName} affix={affix} />
        <QueryResultTable
          display={display as QueryResultTableDisplay}
          table={table}
          propagatedArgs={propagatedArgs}
          setExternalControls={affixRef}
        />
      </>
    );
  }

  if (display[DISPLAY_TYPE_KEY] === GRAPH_DISPLAY_TYPE) {
    return (
      <div className={classes.graphContainer}>
        <WidgetTitlebar title={widgetName} affix={affix} />
        <GraphWidget
          display={display as GraphDisplay}
          data={parsedTable}
          relation={table.relation}
          propagatedArgs={propagatedArgs}
          setExternalControls={affixRef}
        />
      </div>
    );
  }

  if (display[DISPLAY_TYPE_KEY] === REQUEST_GRAPH_DISPLAY_TYPE) {
    return (
      <>
        <WidgetTitlebar title={widgetName} affix={affix} />
        <RequestGraphWidget
          display={display as RequestGraphDisplay}
          data={parsedTable}
          relation={table.relation}
          propagatedArgs={propagatedArgs}
          setExternalControls={affixRef}
        />
      </>
    );
  }

  if (display[DISPLAY_TYPE_KEY] === STAT_CHART_DISPLAY_TYPE) {
    // This one does not get a titlebar, as it puts its title with the text (stylistic choice)
    return (
      <StatChart
        title={widgetName}
        display={display as StatChartDisplay}
        table={table}
        data={parsedTable}
      />
    );
  }

  try {
    return (
      <>
        <WidgetTitlebar title={widgetName} />
        <React.Suspense fallback={<div className={classes.spinner}><Spinner /></div>}>
          <VegaWidget
            tableName={tableName}
            table={table}
            data={parsedTable}
            display={display}
          />
        </React.Suspense>
      </>
    );
  } catch (e) {
    return (
      <div>
        Error in displaySpec:
        {e.message}
      </div>
    );
  }
});
WidgetDisplay.displayName = 'WidgetDisplay';

const ErrorDisplay = React.memo<{ error: any }>(({ error }) => {
  const classes = useStyles();

  const [open, setOpen] = React.useState(false);
  const close = React.useCallback(() => setOpen(false), []);
  React.useEffect(() => {
    setOpen(true);
  }, [error]);

  const vzError = error as VizierQueryError;
  return open && (
    <Alert severity='error' variant='outlined' onClose={close} className={classes.errorDisplay}>
      <AlertTitle>{vzError?.message || error}</AlertTitle>
      <VizierErrorDetails error={error} />
    </Alert>
  );
});
ErrorDisplay.displayName = 'ErrorDisplay';

const Grid = GridLayout.WidthProvider(GridLayout);
const gridMargin = [20, 20];

interface CanvasProps {
  editable: boolean;
  parentRef: React.RefObject<HTMLElement>;
}

const Canvas: React.FC<CanvasProps> = React.memo(({ editable, parentRef }) => {
  const classes = useStyles();
  const {
    tables, loading, error, mutationInfo,
  } = React.useContext(ResultsContext);
  const {
    args, setScriptAndArgs, script,
  } = React.useContext(ScriptContext);
  const { isMobile } = React.useContext(LayoutContext);
  const { setTimeseriesDomain } = React.useContext(TimeSeriesContext);
  const { embedState: { widget } } = React.useContext(LiveRouteContext);
  const isEmbedded = isPixieEmbedded();

  // Default layout used when there is no vis defining widgets.
  const [defaultLayout, setDefaultLayout] = React.useState<Layout[]>([]);
  const [defaultHeight, setDefaultHeight] = React.useState<number>(0);

  // If in embedded mode, send event messages to the parent frame. This is used to track the time from initialization
  // of the live view, to when the script has completed and results are displayed.
  React.useEffect(() => {
    if (isEmbedded) {
      window.top.postMessage({ pixieComponentsInitiated: true }, '*');
    }
  }, [isEmbedded]);
  React.useEffect(() => {
    if (tables.size > 0 && !loading && isEmbedded) {
      window.top.postMessage({ pixieComponentsRendered: true }, '*');
    }
  }, [loading, tables, isEmbedded]);

  const updateDefaultHeight = React.useCallback(() => {
    const newHeight = parentRef.current?.getBoundingClientRect()?.height ?? 0;
    if (newHeight !== 0 && newHeight !== defaultHeight) {
      setDefaultHeight(newHeight);
    }
  }, [defaultHeight, parentRef]);

  React.useEffect(() => {
    /**
     * React-virtualized's AutoSizer works whenever the window receives a resize event. However, this only works with
     * trusted (real) events, synthetic ones don't trigger it. Listening for the untrusted events and manually updating
     * the container's height when that happens is sufficient to fix this.
     */
    window.addEventListener('resize', updateDefaultHeight);
    return () => window.removeEventListener('resize', updateDefaultHeight);
  }, [updateDefaultHeight]);

  if (parentRef.current && !defaultHeight) {
    updateDefaultHeight();
  }

  // These are that we want to propagate to any downstream links in the data table
  // to other live views, such as start time.
  const propagatedArgs = React.useMemo(() => ({
    start_time: args.start_time,
  }), [args]);

  const setVis = React.useCallback(
    (vis: Vis) => setScriptAndArgs({ ...script, vis }, args),
    [args, script, setScriptAndArgs]);

  React.useEffect(() => {
    const newVis = addLayout(script.vis);
    if (newVis !== script.vis) {
      setVis(newVis);
    }
  }, [script, setVis]);

  React.useEffect(() => {
    setTimeseriesDomain(null);
  }, [tables, setTimeseriesDomain]);

  const updateLayoutInVis = React.useCallback((newLayout) => {
    if (!isMobile) {
      const newVis = updatePositions(script.vis, newLayout);
      if (newVis !== script.vis) {
        setVis(newVis);
      }
    }
    triggerResize();
  }, [script?.vis, setVis, isMobile]);

  const updateDefaultLayout = React.useCallback((newLayout) => {
    setDefaultLayout(newLayout);
    triggerResize();
  }, []);

  const className = buildClass(
    'fs-exclude',
    classes.gridItem,
    editable && classes.editable,
    loading && classes.loading,
  );

  const layout = React.useMemo(() => toLayout(script.vis?.widgets, isMobile, widget),
    [script.vis, isMobile, widget]);

  const emptyTableMsg = React.useMemo(() => {
    if (containsMutation(script.code)) {
      return 'Run to generate results';
    }
    return '';
  }, [script?.code]);

  const [affix, setAffix] = React.useState<React.ReactNode>(null);
  const affixRef = React.useCallback((el: React.ReactNode) => { setAffix(el); }, []);

  const charts = React.useMemo(() => {
    const widgets = [];
    script.vis?.widgets.filter((currentWidget) => {
      if (widget) {
        return currentWidget.name === widget;
      }
      return true;
    }).forEach((currentWidget, i) => {
      const widgetLayout = layout[i];
      const display = currentWidget.displaySpec;
      const tableName = widgetTableName(currentWidget, i);
      const widgetName = widgetLayout.i;
      const table = tables.get(tableName);

      if (loading) {
        widgets.push(
          <Paper key={widgetName} className={className} elevation={1}>
            <div className={classes.spinner}><Spinner /></div>
          </Paper>,
        );
        return;
      }

      widgets.push(
        <Paper key={widgetName} className={className}>
          <WidgetDisplay
            display={display}
            table={table}
            tableName={tableName}
            widgetName={widgetName}
            propagatedArgs={propagatedArgs}
            emptyTableMsg={emptyTableMsg}
          />
        </Paper>,
      );
    });
    return widgets;
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [tables, script?.vis, widget, loading, layout, className, classes.spinner, emptyTableMsg]);

  if (loading && charts.length === 0) {
    return (
      // TODO(philkuz) MutationModal is located here and below. Code smell.
      <>
        {loading && mutationInfo && <MutationModal mutationInfo={mutationInfo} />}
        <div className={classes.centerContent}><Spinner /></div>
      </>
    );
  }

  let displayGrid: React.ReactNode;

  if (charts.length === 0) {
    const tableLayout = addTableLayout(Array.from(tables.keys()), defaultLayout, isMobile, defaultHeight);
    displayGrid = (
      <Grid
        layout={tableLayout.layout}
        rowHeight={tableLayout.rowHeight - 40}
        cols={tableLayout.numCols}
        className={classes.grid}
        onLayoutChange={updateDefaultLayout}
        isDraggable={editable}
        isResizable={editable}
        margin={gridMargin}
      >
        {
          Array.from(tables.entries()).map(([tableName, table]) => (
            <Paper elevation={1} key={tableName} className={className}>
              <WidgetTitlebar title={tableName} affix={affix} />
              <QueryResultTable
                display={{} as QueryResultTableDisplay}
                table={table}
                propagatedArgs={propagatedArgs}
                setExternalControls={affixRef}
              />
            </Paper>
          ))
        }
      </Grid>
    );
  } else {
    displayGrid = (
      <Grid
        layout={layout}
        cols={getGridWidth(isMobile)}
        className={classes.grid}
        onLayoutChange={updateLayoutInVis}
        isDraggable={editable}
        isResizable={editable}
        margin={gridMargin}
      >
        {charts}
      </Grid>
    );
  }
  return (
    <>
      {loading && mutationInfo && <MutationModal mutationInfo={mutationInfo} />}
      { error && <ErrorDisplay error={error} />}
      {displayGrid}
    </>
  );
});
Canvas.displayName = 'Canvas';

export default withTimeSeriesContext(Canvas);
