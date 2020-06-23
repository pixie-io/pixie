import 'react-grid-layout/css/styles.css';
import 'react-resizable/css/styles.css';

import clsx from 'clsx';
import { displayToGraph, GraphDisplay } from 'components/chart/graph';
import { Spinner } from 'components/spinner/spinner';
import { VegaContext, withVegaContextProvider } from 'components/vega/vega-context';
import { QueryResultTable } from 'containers/vizier/query-result-viewer';
import * as React from 'react';
import * as GridLayout from 'react-grid-layout';
import { resizeEvent, triggerResize } from 'utils/resize';
import { dataFromProto } from 'utils/result-data-utils';

import { createStyles, makeStyles, Theme, useTheme } from '@material-ui/core/styles';

import { LayoutContext } from './context/layout-context';
import { ResultsContext } from './context/results-context';
import { VisContext } from './context/vis-context';
import {
    addLayout, addTableLayout, getGridWidth, Layout, toLayout, updatePositions,
} from './layout';
import { DISPLAY_TYPE_KEY, GRAPH_DISPLAY_TYPE, TABLE_DISPLAY_TYPE, widgetTableName } from './vis';

const Vega = React.lazy(() => import(
  /* webpackPreload: true */
  /* webpackChunkName: "vega" */
  'components/vega/vega'));

const useStyles = makeStyles((theme: Theme) => {
  return createStyles({
    grid: {
      '& .react-grid-item.react-grid-placeholder': {
        backgroundColor: theme.palette.foreground.grey1,
        borderRadius: theme.spacing(0.5),
      },
    },
    gridItem: {
      padding: theme.spacing(1),
      backgroundColor: theme.palette.background.default,
      borderRadius: theme.spacing(0.5),
      border: `solid 1px ${theme.palette.background.three}`,
      display: 'flex',
      flexDirection: 'column',
      alignItems: 'stretch',
    },
    editable: {
      boxShadow: theme.shadows[10],
      borderColor: theme.palette.foreground.grey2,
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
      ...theme.typography.subtitle1,
      padding: theme.spacing(1),
      borderBottom: `solid 1px ${theme.palette.background.three}`,
    },
    chart: {
      flex: 1,
      minHeight: 0,
    },
    table: {
      '& *': {
        ...theme.typography.body2,
      },
      '& .scrollable-table--row-odd': {
        backgroundColor: theme.palette.background.default,
        outline: [['solid', theme.spacing(0.25), theme.palette.background.three]],
      },
      '& .scrollable-table--row-even': {
        backgroundColor: theme.palette.background.default,
        outline: [['solid', theme.spacing(0.25), theme.palette.background.three]],
      },
      '& .ReactVirtualized__Table__headerRow': {
        borderBottom: [['solid', theme.spacing(0.25), theme.palette.background.three]],
        backgroundColor: theme.palette.background.default,
        '& *': {
          ...theme.typography.caption,
          fontWeight: theme.typography.fontWeightLight,
        },
      },
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
  });
});

const Grid = GridLayout.WidthProvider(GridLayout);

interface CanvasProps {
  editable: boolean;
  parentRef: React.RefObject<HTMLElement>;
}

const Canvas = (props: CanvasProps) => {
  const classes = useStyles();
  const theme = useTheme();
  const { tables, loading } = React.useContext(ResultsContext);
  const { vis, setVis } = React.useContext(VisContext);
  const { isMobile } = React.useContext(LayoutContext);
  const { setTimeseriesDomain } = React.useContext(VegaContext);

  // Default layout used when there is no vis defining widgets.
  const [defaultLayout, setDefaultLayout] = React.useState<Layout[]>([]);
  const [defaultHeight, setDefaultHeight] = React.useState<number>(0);

  if (props.parentRef.current && !defaultHeight) {
    setDefaultHeight(props.parentRef.current.getBoundingClientRect().height);
  }

  React.useEffect(() => {
    const handler = (event) => {
      if (event === resizeEvent || !props.parentRef.current) {
        return;
      }
      setDefaultHeight(props.parentRef.current.getBoundingClientRect().height);
    };
    window.addEventListener('resize', handler);
    return () => window.removeEventListener('resize', handler);
  }, []);

  React.useEffect(() => {
    const newVis = addLayout(vis);
    if (newVis !== vis) {
      setVis(newVis);
    }
  }, [vis]);

  React.useEffect(() => {
    setTimeseriesDomain(null);
  }, [tables]);

  const updateLayoutInVis = React.useCallback((newLayout) => {
    if (!isMobile) {
      setVis(updatePositions(vis, newLayout));
    }
    triggerResize();
  }, [vis, isMobile]);

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

  const layout = React.useMemo(() => {
    return toLayout(vis.widgets, isMobile);
  }, [vis, isMobile]);

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
      let content = null;

      if (loading) {
        widgets.push(
            <div key={widgetName} className={className}>
              <div className={classes.spinner}><Spinner /></div>
            </div>,
        );
        return;
      }

      if (!table) {
        return;
      } else if (display[DISPLAY_TYPE_KEY] === TABLE_DISPLAY_TYPE) {
        content = <>
          <div className={classes.widgetTitle}>{widgetName}</div>
          <QueryResultTable className={classes.table} data={table} />
        </>;
      } else if (display[DISPLAY_TYPE_KEY] === GRAPH_DISPLAY_TYPE) {
        const parsedTable = dataFromProto(table.relation, table.data);
        content = displayToGraph(display as GraphDisplay, parsedTable);
      } else {
        try {
          const data = dataFromProto(table.relation, table.data);
          content = <>
            <div className={classes.widgetTitle}>{widgetName}</div>
            <React.Suspense fallback={<div className={classes.spinner}><Spinner /></div>}>
              <Vega
                className={classes.chart}
                data={data}
                display={display as React.ComponentProps<typeof Vega>['display']}
                tableName={tableName}
              />
            </React.Suspense>
          </>;
        } catch (e) {
          content = <div>Error in displaySpec: {e.message}</div>;
        }
      }
      widgets.push(
        <div key={widgetName} className={className}>
          {content}
        </div>,
      );
    });
    return widgets;
  }, [tables, vis, loading, layout]);

  if (loading && charts.length === 0) {
    return (
      <div className='center-content'><Spinner /></div>
    );
  }

  if (charts.length === 0) {
    const { layout, numCols, rowHeight } = addTableLayout(Object.keys(tables), defaultLayout, isMobile, defaultHeight);
    return (
      <Grid
        layout={layout}
        rowHeight={rowHeight - theme.spacing(5)}
        cols={numCols}
        className={classes.grid}
        onLayoutChange={updateDefaultLayout}
        isDraggable={props.editable}
        isResizable={props.editable}
        margin={[theme.spacing(2.5), theme.spacing(2.5)]}
      >
        {
          Object.entries(tables).map(([tableName, table]) => (
            <div key={tableName} className={className}>
              <div className={classes.widgetTitle}>{tableName}</div>
              <QueryResultTable className={classes.table} data={table} />
            </div>
          ))
        }
      </Grid >
    );
  }

  return (
    <Grid
      layout={layout}
      cols={getGridWidth(isMobile)}
      className={classes.grid}
      onLayoutChange={updateLayoutInVis}
      isDraggable={props.editable}
      isResizable={props.editable}
      margin={[theme.spacing(2.5), theme.spacing(2.5)]}
    >
      {charts}
    </Grid >
  );
};

export default withVegaContextProvider(Canvas);
