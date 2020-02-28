import 'react-grid-layout/css/styles.css';
import 'react-resizable/css/styles.css';

import {parseSpecs} from 'components/vega/spec';
import Vega from 'components/vega/vega';
import * as React from 'react';
import * as GridLayout from 'react-grid-layout';
import {AutoSizer} from 'react-virtualized';

import {LiveContext, PlacementContext, ResultsContext, VegaContext} from './context';
import {buildLayout, parsePlacement, Placement, toLayout, updatePositions} from './layout';

const Canvas = () => {
  const inputJSON = React.useContext(VegaContext);
  const results = React.useContext(ResultsContext);
  const placementJSON = React.useContext(PlacementContext);
  const { updatePlacement: updatePlacementJSON } = React.useContext(LiveContext);

  const [placement, setPlacement] = React.useState<Placement>({});

  const specs = React.useMemo(() => parseSpecs(inputJSON), [inputJSON]);

  // Parse the placement only once on init. This is to avoid infinte loops.
  React.useEffect(() => {
    try {
      const initialPlacement = parsePlacement(placementJSON);
      setPlacement(buildLayout(specs, initialPlacement));
    } catch (e) {
      // noop. tslint doesn't allow empty blocks.
    }
  }, []);

  const charts = React.useMemo(() => {
    return Object.keys(specs).map((chartName) => {
      const spec = specs[chartName];
      const tableName = spec.data && (spec.data as { name: string }).name || 'output';
      const data = results[tableName] || [];
      return (
        <div key={chartName}>
          <Vega data={data} spec={spec} tableName={tableName} />;
        </div>
      );
    });
  }, [results, specs]);

  const layout = React.useMemo(() => toLayout(placement), [placement]);

  const resize = React.useCallback(() => {
    // Dispatch a window resize event to signal the chart to redraw. As suggested in:
    // https://vega.github.io/vega-lite/docs/size.html#specifying-responsive-width-and-height
    window.dispatchEvent(new Event('resize'));
  }, []);

  const handleLayoutChange = React.useCallback((newLayout) => {
    updatePlacementJSON(JSON.stringify(updatePositions(placement, newLayout)));
    resize();
  }, []);

  return (
    <AutoSizer onResize={resize}>
      {({ height, width }) => (
        <GridLayout
          style={{ width, height }}
          width={width}
          layout={layout}
          onLayoutChange={handleLayoutChange}
        >
          {charts}
        </GridLayout>
      )}
    </AutoSizer >
  );
};

export default Canvas;
