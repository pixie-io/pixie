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

import { buildClass, Spinner } from 'app/components';
import { GraphDisplay, GraphWidget } from 'app/containers/live-widgets/graph/graph';
import { RequestGraphDisplay, RequestGraphWidget } from 'app/containers/live-widgets/graph/request-graph';

import {
  TimeSeriesContext, withTimeSeriesContextProvider,
} from 'app/containers/live-widgets/context/time-series-context';
import { QueryResultTableDisplay, QueryResultTable } from 'app/containers/live-widgets/table/query-result-viewer';
import * as React from 'react';
import * as GridLayout from 'react-grid-layout';
import { resizeEvent, triggerResize } from 'app/utils/resize';
import { dataFromProto } from 'app/utils/result-data-utils';
import { Alert, AlertTitle } from '@material-ui/core';
import { VizierQueryError, Table as VizierTable } from 'app/api';
import { containsMutation } from 'app/utils/pxl';
import { VizierErrorDetails } from 'app/common/errors';

import {
  alpha, makeStyles, Theme,
} from '@material-ui/core/styles';
import { createStyles } from '@material-ui/styles';
import Paper from '@material-ui/core/Paper';

import Vega from 'app/containers/live-widgets/vega/vega';
import { LiveRouteContext } from 'app/containers/App/live-routing';
import { LayoutContext } from 'app/context/layout-context';
import { ResultsContext } from 'app/context/results-context';
import { ScriptContext } from 'app/context/script-context';
import MutationModal from './mutation-modal';
import {
  addLayout, addTableLayout, getGridWidth, Layout, toLayout, updatePositions,
} from './layout';
import {
  DISPLAY_TYPE_KEY, GRAPH_DISPLAY_TYPE, REQUEST_GRAPH_DISPLAY_TYPE,
  TABLE_DISPLAY_TYPE, Vis, widgetTableName, WidgetDisplay as VisWidgetDisplay,
} from './vis';

const useStyles = makeStyles((theme: Theme) => createStyles({
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
  widgetTitle: {
    ...theme.typography.h3,
    padding: theme.spacing(0.5),
    color: theme.palette.foreground.two,
    textTransform: 'capitalize',
    marginBottom: theme.spacing(1.8),
  },
  chart: {
    flex: 1,
    minHeight: 0,
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
    position: 'absolute',
    zIndex: 1, // Position on top of canvas.
    height: '100%',
    width: '100%',
  },
  error: {
    position: 'absolute',
    '& .MuiAlert-root': {
      padding: theme.spacing(3),
    },
    '& .MuiAlert-icon': {
      paddingTop: theme.spacing(0.6),
    },
    '& .MuiAlert-action': {
      display: 'block',
      color: theme.palette.text.secondary,
      opacity: 0.65,
    },
    marginTop: theme.spacing(5),
    marginLeft: theme.spacing(5),
  },
  errorOverlay: {
    width: '100%',
    height: '100%',
    position: 'absolute',
  },
  errorTitle: {
    ...theme.typography.body2,
    fontFamily: '"Roboto Mono", Monospace',
    color: theme.palette.error.dark,
    paddingBottom: theme.spacing(3),
  },
  hidden: {
    display: 'none',
  },
  blur: {
    filter: `blur(${theme.spacing(0.2)}px)`,
  },
  graphContainer: {
    display: 'flex',
    flexDirection: 'column',
    height: '100%',
  },
  vegaWidget: {
    flex: 1,
    display: 'flex',
    border: '1px solid transparent',
    '&.focus': {
      border: `1px solid ${theme.palette.foreground.grey2}`,
    },
  },
}), { name: 'Canvas' });

interface VegaWidgetProps {
  tableName: string;
  display: VisWidgetDisplay;
  table: VizierTable;
  data: any[];
}

const VegaWidget: React.FC<VegaWidgetProps> = ({
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
};

const WidgetDisplay = ({
  display, table, tableName, widgetName, propagatedArgs, emptyTableMsg,
}) => {
  const classes = useStyles();

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
        <div className={classes.widgetTitle}>{widgetName}</div>
        <QueryResultTable
          display={display as QueryResultTableDisplay}
          data={table}
          propagatedArgs={propagatedArgs}
        />
      </>
    );
  }

  const parsedTable = dataFromProto(table.relation, table.data);

  if (display[DISPLAY_TYPE_KEY] === GRAPH_DISPLAY_TYPE) {
    return (
      <div className={classes.graphContainer}>
        <div className={classes.widgetTitle}>{widgetName}</div>
        <GraphWidget
          display={display as GraphDisplay}
          data={parsedTable}
          relation={table.relation}
          propagatedArgs={propagatedArgs}
        />
      </div>
    );
  }

  if (display[DISPLAY_TYPE_KEY] === REQUEST_GRAPH_DISPLAY_TYPE) {
    return (
      <>
        <div className={classes.widgetTitle}>{widgetName}</div>
        <RequestGraphWidget
          display={display as RequestGraphDisplay}
          data={parsedTable}
          relation={table.relation}
          propagatedArgs={propagatedArgs}
        />
      </>
    );
  }

  try {
    return (
      <>
        <div className={classes.widgetTitle}>{widgetName}</div>
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
};

const ErrorDisplay = (props) => {
  const { error, setOpen } = props;
  const toggleOpen = React.useCallback(() => setOpen((opened) => !opened), [setOpen]);
  React.useEffect(() => {
    setOpen(true);
  }, [error, setOpen]);

  const vzError = props.error as VizierQueryError;
  return (
    <div className={props.open ? props.classes.errorDisplay : props.classes.hidden}>
      <div className={props.classes.errorOverlay} />
      <div className={props.classes.error}>
        <Alert severity='error' onClose={toggleOpen}>
          <AlertTitle className={props.classes.errorTitle}>{vzError?.message || props.error}</AlertTitle>
          <VizierErrorDetails error={props.error} />
        </Alert>
      </div>
    </div>
  );
};

const Grid = GridLayout.WidthProvider(GridLayout);

interface CanvasProps {
  editable: boolean;
  parentRef: React.RefObject<HTMLElement>;
}

const Canvas = (props: CanvasProps) => {
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

  // Default layout used when there is no vis defining widgets.
  const [defaultLayout, setDefaultLayout] = React.useState<Layout[]>([]);
  const [defaultHeight, setDefaultHeight] = React.useState<number>(0);

  React.useEffect(() => {
    /**
     * React-virtualized's AutoSizer works whenever the window receives a resize event. However, this only works with
     * trusted (real) events, synthetic ones don't trigger it. Listening for the untrusted events and manually updating
     * the container's height when that happens is sufficient to fix this.
     */
    const listener = (event: Event) => {
      if (event.type === 'resize' && !event.isTrusted) {
        setDefaultHeight(props.parentRef.current.getBoundingClientRect().height);
      }
    };
    window.addEventListener('resize', listener);
    return () => window.removeEventListener('resize', listener);
  }, [props.parentRef]);

  if (props.parentRef.current && !defaultHeight) {
    const newHeight = props.parentRef.current.getBoundingClientRect().height;
    if (newHeight !== defaultHeight) {
      setDefaultHeight(newHeight);
    }
  }

  // These are that we want to propagate to any downstream links in the data table
  // to other live views, such as start time.
  const propagatedArgs = React.useMemo(() => ({
    start_time: args.start_time,
  }), [args]);

  React.useEffect(() => {
    const handler = (event) => {
      if (event === resizeEvent || !props.parentRef.current) {
        return;
      }
      setDefaultHeight(props.parentRef.current.getBoundingClientRect().height);
    };
    window.addEventListener('resize', handler);
    return () => window.removeEventListener('resize', handler);
  }, [props.parentRef, setDefaultHeight]);

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
    props.editable && classes.editable,
    loading && classes.loading,
  );

  const layout = React.useMemo(() => toLayout(script.vis?.widgets, isMobile, widget),
    [script.vis, isMobile, widget]);
  const [errorOpen, setErrorOpen] = React.useState(false);

  const emptyTableMsg = React.useMemo(() => {
    if (containsMutation(script.code)) {
      return 'Run to generate results';
    }
    return '';
  }, [script?.code]);

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
      const table = tables[tableName];

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
        <div className='center-content'><Spinner /></div>
      </>
    );
  }

  let displayGrid: React.ReactNode;

  if (charts.length === 0) {
    const tableLayout = addTableLayout(Object.keys(tables), defaultLayout, isMobile, defaultHeight);
    displayGrid = (
      <Grid
        layout={tableLayout.layout}
        rowHeight={tableLayout.rowHeight - 40}
        cols={tableLayout.numCols}
        className={buildClass(classes.grid, errorOpen && error && classes.blur)}
        onLayoutChange={updateDefaultLayout}
        isDraggable={props.editable}
        isResizable={props.editable}
        margin={[20, 20]}
      >
        {
          Object.entries(tables).map(([tableName, table]) => (
            <Paper elevation={1} key={tableName} className={className}>
              <div className={classes.widgetTitle}>{tableName}</div>
              <QueryResultTable display={{} as QueryResultTableDisplay} data={table} propagatedArgs={propagatedArgs} />
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
        className={buildClass(classes.grid, errorOpen && error && classes.blur)}
        onLayoutChange={updateLayoutInVis}
        isDraggable={props.editable}
        isResizable={props.editable}
        margin={[20, 20]}
      >
        {charts}
      </Grid>
    );
  }
  return (
    <>
      {loading && mutationInfo && <MutationModal mutationInfo={mutationInfo} />}
      { error
        && <ErrorDisplay classes={classes} error={error} setOpen={setErrorOpen} open={errorOpen} />}
      {displayGrid}
    </>
  );
};

export default withTimeSeriesContextProvider(Canvas);
