import 'react-grid-layout/css/styles.css';
import 'react-resizable/css/styles.css';

import {Spinner} from 'components/spinner/spinner';
import {parseSpecs} from 'components/vega/spec';
import Vega from 'components/vega/vega';
import {QueryResultTable} from 'containers/vizier/query-result-viewer';
import * as React from 'react';
import * as GridLayout from 'react-grid-layout';
import {dataFromProto} from 'utils/result-data-utils';

import {createStyles, makeStyles, Theme, useTheme} from '@material-ui/core/styles';

import {LiveContext, PlacementContext, ResultsContext, VegaContext} from './context';
import {buildLayout, toLayout, updatePositions} from './layout';

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
  const classes = useStyles();
  const specs = React.useContext(VegaContext);
  const results = React.useContext(ResultsContext);
  const placement = React.useContext(PlacementContext);
  const { updatePlacement } = React.useContext(LiveContext);
  const [vegaModule, setVegaModule] = React.useState(null);

  // Load vega.
  React.useEffect(() => {
    import(/* webpackChunkName: "react-vega" webpackPreload: true */ 'react-vega').then((module) => {
      setVegaModule(module);
    });
  }, []);

  React.useEffect(() => {
    const newPlacement = buildLayout(specs, placement);
    if (newPlacement !== placement) {
      updatePlacement(newPlacement);
    }
  }, [specs]);

  const layout = React.useMemo(() => {
    return toLayout(placement);
  }, [placement]);

  const charts = React.useMemo(() => {
    if (!vegaModule) {
      return [];
    }
    return Object.keys(specs).map((chartName) => {
      const spec = specs[chartName];
      const tableName = spec.data && (spec.data as { name: string }).name || 'output';
      const table = results[tableName];
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
          <Vega data={data} spec={spec} tableName={tableName} vegaModule={vegaModule} />
        </div>
      );
    });
  }, [results, specs, vegaModule]);

  const resize = React.useCallback(() => {
    // Dispatch a window resize event to signal the chart to redraw. As suggested in:
    // https://vega.github.io/vega-lite/docs/size.html#specifying-responsive-width-and-height
    window.dispatchEvent(new Event('resize'));
  }, []);

  const handleLayoutChange = React.useCallback((newLayout) => {
    updatePlacement(updatePositions(placement, newLayout));
    resize();
  }, []);

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
