import 'react-grid-layout/css/styles.css';
import 'react-resizable/css/styles.css';

import clsx from 'clsx';
import { displayToGraph, GraphDisplay } from 'components/chart/graph';
import { Spinner } from 'components/spinner/spinner';
import { parseSpecs } from 'components/vega/spec';
import { addPxTimeFormatExpression } from 'components/vega/timeseries-axis';
import Vega from 'components/vega/vega';
import { QueryResultTable } from 'containers/vizier/query-result-viewer';
import * as React from 'react';
import * as GridLayout from 'react-grid-layout';
import { dataFromProto } from 'utils/result-data-utils';

import { createStyles, makeStyles, Theme, useTheme } from '@material-ui/core/styles';

import { LiveContext, ResultsContext, VisContext } from './context';
import { ChartDisplay, convertWidgetDisplayToVegaSpec } from './convert-to-vega-spec';
import { addLayout, toLayout, updatePositions } from './layout';
import { DISPLAY_TYPE_KEY, GRAPH_DISPLAY_TYPE, TABLE_DISPLAY_TYPE, widgetResultName } from './vis';

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
        color: `${theme.palette.text.primary} !important`,
      },
      '& .scrollable-table--row-odd': {
        backgroundColor: theme.palette.background.default,
        borderBottom: [['solid', theme.spacing(0.25), theme.palette.background.three]],
      },
      '& .scrollable-table--row-even': {
        backgroundColor: theme.palette.background.default,
        borderBottom: [['solid', theme.spacing(0.25), theme.palette.background.three]],
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
  });
});

const Grid = GridLayout.WidthProvider(GridLayout);

interface CanvasProps {
  editable: boolean;
}

const Canvas = (props: CanvasProps) => {
  const classes = useStyles();
  const theme = useTheme();
  const { tables } = React.useContext(ResultsContext);
  const vis = React.useContext(VisContext);
  const { updateVis } = React.useContext(LiveContext);
  const [vegaModule, setVegaModule] = React.useState(null);
  const [reactVegaModule, setReactVegaModule] = React.useState(null);
  const [vegaLiteModule, setVegaLiteModule] = React.useState(null);

  // Load react-vega.
  React.useEffect(() => {
    import(/* webpackChunkName: "react-vega" webpackPreload: true */ 'react-vega').then((module) => {
      setReactVegaModule(module);
    });
  }, []);

  // Load vega.
  React.useEffect(() => {
    import(/* webpackChunkName: "vega" webpackPreload: true */ 'vega').then((module) => {
      setVegaModule(module);
    });
  }, []);

  // Load vega-lite.
  React.useEffect(() => {
    import(/* webpackChunkName: "vega-lite" webpackPreload: true */ 'vega-lite').then((module) => {
      setVegaLiteModule(module);
    });
  }, []);

  const layout = React.useMemo(() => {
    const newVis = addLayout(vis);
    if (newVis !== vis) {
      updateVis(newVis);
    }
    return toLayout(newVis);
  }, [vis]);

  const charts = React.useMemo(() => {
    if (!reactVegaModule || !vegaModule) {
      return [];
    }
    return vis.widgets.map((widget, i) => {
      const display = widget.displaySpec;
      // TODO(nserrino): Support multiple output tables when we have a Vega component that
      // takes in multiple output tables.
      const name = widgetResultName(widget, i);
      const table = tables[name];
      const className = clsx(
        'fs-exclude',
        classes.gridItem,
        props.editable && classes.editable,
      );
      let content = null;
      if (!table) {
        content = <div key={name}>Table {name} not found.</div>;
      } else if (display[DISPLAY_TYPE_KEY] === TABLE_DISPLAY_TYPE) {
        content = <>
          <div className={classes.widgetTitle}>{name}</div>
          <QueryResultTable className={classes.table} data={table} />
        </>;
      } else if (display[DISPLAY_TYPE_KEY] === GRAPH_DISPLAY_TYPE) {
        const parsedTable = dataFromProto(table.relation, table.data);
        content = displayToGraph(display as GraphDisplay, parsedTable);
      } else {
        try {
          const spec = convertWidgetDisplayToVegaSpec(display as ChartDisplay, name, theme, vegaLiteModule);
          const data = dataFromProto(table.relation, table.data);
          addPxTimeFormatExpression(vegaModule);
          content = <>
            <div className={classes.widgetTitle}>{name}</div>
            <Vega
              className={classes.chart}
              data={data}
              spec={spec}
              tableName={name}
              reactVegaModule={reactVegaModule}
            />
          </>;
        } catch (e) {
          content = <div>Error in displaySpec: {e.message}</div>;
        }
      }
      return <div key={name} className={className}>{content}</div>;
    });
  }, [tables, vis, reactVegaModule, props.editable]);

  const resize = React.useCallback(() => {
    // Dispatch a window resize event to signal the chart to redraw. As suggested in:
    // https://vega.github.io/vega-lite/docs/size.html#specifying-responsive-width-and-height
    window.dispatchEvent(new Event('resize'));
  }, []);

  const handleLayoutChange = React.useCallback((newLayout) => {
    updateVis(updatePositions(vis, newLayout));
    resize();
  }, [vis]);

  if (!reactVegaModule) {
    return (
      <div className='center-content'><Spinner /></div>
    );
  }

  return (
    <Grid
      className={classes.grid}
      layout={layout}
      onLayoutChange={handleLayoutChange}
      isDraggable={props.editable}
      isResizable={props.editable}
      margin={[theme.spacing(2.5), theme.spacing(2.5)]}
    >
      {charts}
    </Grid>
  );
};

export default Canvas;
