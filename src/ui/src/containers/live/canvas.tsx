import 'react-grid-layout/css/styles.css';
import 'react-resizable/css/styles.css';

import {displayToGraph, GraphDisplay} from 'components/chart/graph';
import {Spinner} from 'components/spinner/spinner';
import {parseSpecs} from 'components/vega/spec';
import Vega from 'components/vega/vega';
import {QueryResultTable} from 'containers/vizier/query-result-viewer';
import * as React from 'react';
import * as GridLayout from 'react-grid-layout';
import {dataFromProto} from 'utils/result-data-utils';

import {createStyles, makeStyles, Theme, useTheme} from '@material-ui/core/styles';

import {
    LiveContext, PlacementContextOld, ResultsContext, VegaContextOld, VisContext,
} from './context';
import {ChartDisplay, convertWidgetDisplayToVegaSpec} from './convert-to-vega-spec';
import {
    addLayout, buildLayoutOld, toLayout, toLayoutOld, updatePositions, updatePositionsOld,
} from './layout';
import {DISPLAY_TYPE_KEY, GRAPH_DISPLAY_TYPE, TABLE_DISPLAY_TYPE, widgetResultName} from './vis';

const useStyles = makeStyles((theme: Theme) => {
  return createStyles({
    table: {
      '& *': {
        ...theme.typography.body2,
        color: `${theme.palette.text.primary} !important`,
      },
      '& .scrollable-table--row-odd': {
        backgroundColor: theme.palette.background.two,
        borderBottom: [['solid', theme.spacing(0.25), theme.palette.background.three]],
      },
      '& .scrollable-table--row-even': {
        backgroundColor: theme.palette.background.two,
        borderBottom: [['solid', theme.spacing(0.25), theme.palette.background.three]],
      },
      '& .ReactVirtualized__Table__headerRow': {
        backgroundColor: theme.palette.background.one,
        '& *': {
          ...theme.typography.caption,
          fontWeight: theme.typography.fontWeightLight,
        },
      },
    },
  });
});

const Grid = GridLayout.WidthProvider(GridLayout);

const Canvas = () => {
  const { oldLiveViewMode } = React.useContext(LiveContext);
  if (oldLiveViewMode) {
    return <OldCanvas />;
  } else {
    return <NewCanvas />;
  }
};

const NewCanvas = () => {
  const classes = useStyles();
  const { tables } = React.useContext(ResultsContext);
  const vis = React.useContext(VisContext);
  const { updateVis } = React.useContext(LiveContext);
  const [vegaModule, setVegaModule] = React.useState(null);

  // Load vega.
  React.useEffect(() => {
    import(/* webpackChunkName: "react-vega" webpackPreload: true */ 'react-vega').then((module) => {
      setVegaModule(module);
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
    if (!vegaModule) {
      return [];
    }
    return vis.widgets.map((widget, i) => {
      const display = widget.displaySpec;
      // TODO(nserrino): Support multiple output tables when we have a Vega component that
      // takes in multiple output tables.
      const name = widgetResultName(widget, i);
      const table = tables[name];
      if (!table) {
        return <div key={name}>Table {name} not found.</div>;
      }

      if (display[DISPLAY_TYPE_KEY] === TABLE_DISPLAY_TYPE) {
        return (
          <div key={name} className='fs-exclude'>
            <QueryResultTable className={classes.table} data={table} />
          </div>
        );
      }
      if (display[DISPLAY_TYPE_KEY] === GRAPH_DISPLAY_TYPE) {
        const parsedTable = dataFromProto(table.relation, table.data);
        return (
          <div key={name} className='fs-exclude'>
            {displayToGraph(display as GraphDisplay, parsedTable)}
          </div>
        );
      }
      let spec;
      try {
        spec = convertWidgetDisplayToVegaSpec(display as ChartDisplay, name);
      } catch (e) {
        return <div key={name} className='fs-exclude'>Error in displaySpec: {e.message}</div>;
      }
      const data = dataFromProto(table.relation, table.data);
      return (
        <div key={name} className='fs-exclude'>
          <Vega data={data} spec={spec} tableName={name} oldSpec={false} vegaModule={vegaModule} />
        </div>
      );
    });
  }, [tables, vis, vegaModule]);

  const resize = React.useCallback(() => {
    // Dispatch a window resize event to signal the chart to redraw. As suggested in:
    // https://vega.github.io/vega-lite/docs/size.html#specifying-responsive-width-and-height
    window.dispatchEvent(new Event('resize'));
  }, []);

  const handleLayoutChange = React.useCallback((newLayout) => {
    updateVis(updatePositions(vis, newLayout));
    resize();
  }, [vis]);

  if (!vegaModule) {
    return (
      <div className='center-content'><Spinner /></div>
    );
  }

  return (
    <Grid layout={layout} onLayoutChange={handleLayoutChange}>
      {charts}
    </Grid>
  );
};

const OldCanvas = () => {
  const classes = useStyles();
  const specs = React.useContext(VegaContextOld);
  const { tables } = React.useContext(ResultsContext);
  const placement = React.useContext(PlacementContextOld);
  const { updatePlacementOld } = React.useContext(LiveContext);
  const [vegaModule, setVegaModule] = React.useState(null);

  // Load vega.
  React.useEffect(() => {
    import(/* webpackChunkName: "react-vega" webpackPreload: true */ 'react-vega').then((module) => {
      setVegaModule(module);
    });
  }, []);

  React.useEffect(() => {
    const newPlacement = buildLayoutOld(specs, placement);
    if (newPlacement !== placement) {
      updatePlacementOld(newPlacement);
    }
  }, [specs]);

  const layout = React.useMemo(() => {
    return toLayoutOld(placement);
  }, [placement]);

  const charts = React.useMemo(() => {
    if (!vegaModule) {
      return [];
    }
    return Object.keys(specs).map((chartName) => {
      const spec = specs[chartName];
      const tableName = spec.data && (spec.data as { name: string }).name || 'output';
      const table = tables[tableName];
      if (!table) {
        return <div key={chartName}>Table {tableName} not found.</div>;
      }
      if ((spec as { mark: string }).mark === 'table') {
        return (
          <div key={chartName} className='fs-exclude'>
            <QueryResultTable className={classes.table} data={table} />
          </div>
        );
      }
      const data = dataFromProto(table.relation, table.data);
      return (
        <div key={chartName} className='fs-exclude'>
          <Vega data={data} spec={spec} tableName={tableName} oldSpec={true} vegaModule={vegaModule} />
        </div>
      );
    });
  }, [tables, specs, vegaModule]);

  const resize = React.useCallback(() => {
    // Dispatch a window resize event to signal the chart to redraw. As suggested in:
    // https://vega.github.io/vega-lite/docs/size.html#specifying-responsive-width-and-height
    window.dispatchEvent(new Event('resize'));
  }, []);

  const handleLayoutChange = React.useCallback((newLayout) => {
    updatePlacementOld(updatePositionsOld(placement, newLayout));
    resize();
  }, [placement]);

  if (!vegaModule) {
    return (
      <div className='center-content'><Spinner /></div>
    );
  }

  return (
    <Grid layout={layout} onLayoutChange={handleLayoutChange}>
      {charts}
    </Grid>
  );
};

export default Canvas;
