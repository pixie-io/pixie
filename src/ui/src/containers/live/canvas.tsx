import 'react-grid-layout/css/styles.css';
import 'react-resizable/css/styles.css';

import clsx from 'clsx';
import { GraphDisplay, GraphWidget } from 'components/live-widgets/graph/graph';
import { RequestGraphDisplay, RequestGraphWidget } from 'components/live-widgets/graph/request-graph';
import { Spinner } from 'components/spinner/spinner';
import { TimeSeriesContext, withTimeSeriesContextProvider } from 'components/live-widgets/context/time-series-context';
import { QueryResultTable } from 'components/live-widgets/table/query-result-viewer';
import * as React from 'react';
import * as GridLayout from 'react-grid-layout';
import { resizeEvent, triggerResize } from 'utils/resize';
import { dataFromProto } from 'utils/result-data-utils';
import { Alert, AlertTitle } from '@material-ui/lab';
import { VizierErrorDetails, VizierQueryError } from 'common/errors';
import { ContainsMutation } from 'utils/pxl';

import {
  createStyles, fade, makeStyles, Theme, useTheme,
} from '@material-ui/core/styles';

import Vega from 'components/live-widgets/vega/vega';
import MutationModal from './mutation-modal';
import { LayoutContext } from '../../context/layout-context';
import { ResultsContext } from '../../context/results-context';
import { ScriptContext } from '../../context/script-context';
import {
  addLayout, addTableLayout, getGridWidth, Layout, toLayout, updatePositions,
} from './layout';
import {
  DISPLAY_TYPE_KEY, GRAPH_DISPLAY_TYPE, REQUEST_GRAPH_DISPLAY_TYPE,
  TABLE_DISPLAY_TYPE, widgetTableName,
} from './vis';

const useStyles = makeStyles((theme: Theme) => createStyles({
  grid: {
    '& .react-grid-item.react-grid-placeholder': {
      backgroundColor: theme.palette.foreground.grey1,
      borderRadius: theme.spacing(0.5),
    },
  },
  gridItem: {
    padding: theme.spacing(0.8),
    backgroundColor: theme.palette.background.six,
    boxShadow: '2px 2px 2px rgba(0, 0, 0, 0.31)',
    borderRadius: '5px',
    border: theme.palette.border.unFocused,
    display: 'flex',
    flexDirection: 'column',
    alignItems: 'stretch',
  },
  editable: {
    boxShadow: theme.shadows[10],
    borderColor: fade(theme.palette.primary.dark, 0.50),
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
}));

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
        <QueryResultTable data={table} propagatedArgs={propagatedArgs} />
      </>
    );
  }

  const parsedTable = dataFromProto(table.relation, table.data);

  if (display[DISPLAY_TYPE_KEY] === GRAPH_DISPLAY_TYPE) {
    return (
      <>
        <div className={classes.widgetTitle}>{widgetName}</div>
        <GraphWidget
          display={display as GraphDisplay}
          data={parsedTable}
          relation={table.relation}
          propagatedArgs={propagatedArgs}
        />
      </>
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
          <Vega
            className={classes.chart}
            data={parsedTable}
            display={display as React.ComponentProps<typeof Vega>['display']}
            relation={table.relation}
            tableName={tableName}
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
  const theme = useTheme();
  const {
    tables, loading, error, mutationInfo,
  } = React.useContext(ResultsContext);
  const {
    args, vis, setVis, pxl,
  } = React.useContext(ScriptContext);
  const { isMobile } = React.useContext(LayoutContext);
  const { setTimeseriesDomain } = React.useContext(TimeSeriesContext);

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
  const propagatedArgs = React.useMemo(() => {
    if (args.start_time) {
      // eslint-disable-next-line @typescript-eslint/camelcase
      return { start_time: args.start_time };
    }
    return null;
  }, [args]);

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

  React.useEffect(() => {
    const newVis = addLayout(vis);
    if (newVis !== vis) {
      setVis(newVis);
    }
  }, [vis, setVis]);

  React.useEffect(() => {
    setTimeseriesDomain(null);
  }, [tables, setTimeseriesDomain]);

  const updateLayoutInVis = React.useCallback((newLayout) => {
    if (!isMobile) {
      setVis(updatePositions(vis, newLayout));
    }
    triggerResize();
  }, [vis, setVis, isMobile]);

  const updateDefaultLayout = React.useCallback((newLayout) => {
    setDefaultLayout(newLayout);
    triggerResize();
  }, []);

  const className = clsx(
    'fs-exclude',
    classes.gridItem,
    props.editable && classes.editable,
    loading && classes.loading,
  );

  const layout = React.useMemo(() => toLayout(vis.widgets, isMobile), [vis, isMobile]);
  const [errorOpen, setErrorOpen] = React.useState(false);

  const emptyTableMsg = React.useMemo(() => {
    if (ContainsMutation(pxl)) {
      return 'Run to generate results';
    }
    return '';
  }, [pxl]);

  const charts = React.useMemo(() => {
    const widgets = [];
    vis.widgets.forEach((widget, i) => {
      const widgetLayout = layout[i];
      const display = widget.displaySpec;
      // TODO(nserrino): Support multiple output tables when we have a Vega component that
      // takes in multiple output tables.
      const tableName = widgetTableName(widget, i);
      const widgetName = widgetLayout.i;
      const table = tables[tableName];

      if (loading) {
        widgets.push(
          <div key={widgetName} className={className}>
            <div className={classes.spinner}><Spinner /></div>
          </div>,
        );
        return;
      }

      widgets.push(
        <div key={widgetName} className={className}>
          <WidgetDisplay
            display={display}
            table={table}
            tableName={tableName}
            widgetName={widgetName}
            propagatedArgs={propagatedArgs}
            emptyTableMsg={emptyTableMsg}
          />
        </div>,
      );
    });
    return widgets;
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [tables, vis, loading, layout, className, classes.spinner, emptyTableMsg]);

  if (loading && charts.length === 0) {
    return (
      <>
        {loading && mutationInfo && <MutationModal mutationInfo={mutationInfo} /> }
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
        rowHeight={tableLayout.rowHeight - theme.spacing(5)}
        cols={tableLayout.numCols}
        className={clsx(classes.grid, errorOpen && error && classes.blur)}
        onLayoutChange={updateDefaultLayout}
        isDraggable={props.editable}
        isResizable={props.editable}
        margin={[theme.spacing(2.5), theme.spacing(2.5)]}
      >
        {
          Object.entries(tables).map(([tableName, table]) => (
            <div key={tableName} className={className}>
              <div className={classes.widgetTitle}>{tableName}</div>
              <QueryResultTable data={table} propagatedArgs={propagatedArgs} />
            </div>
          ))
        }
      </Grid>
    );
  } else {
    displayGrid = (
      <Grid
        layout={layout}
        cols={getGridWidth(isMobile)}
        className={clsx(classes.grid, errorOpen && error && classes.blur)}
        onLayoutChange={updateLayoutInVis}
        isDraggable={props.editable}
        isResizable={props.editable}
        margin={[theme.spacing(2.5), theme.spacing(2.5)]}
      >
        {charts}
      </Grid>
    );
  }
  return (
    <>
      {loading && mutationInfo && <MutationModal mutationInfo={mutationInfo} /> }
      { error
        && <ErrorDisplay classes={classes} error={error} setOpen={setErrorOpen} open={errorOpen} /> }
      {displayGrid}
    </>
  );
};

export default withTimeSeriesContextProvider(Canvas);
