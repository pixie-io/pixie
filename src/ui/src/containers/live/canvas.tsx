import {parseSpecs} from 'components/vega/spec';
import Vega from 'components/vega/vega';
import * as React from 'react';
import {AutoSizer} from 'react-virtualized';

import {ResultsContext, VegaContext} from './context';

const Canvas = () => {
  const inputJSON = React.useContext(VegaContext);
  const results = React.useContext(ResultsContext);

  const spec = React.useMemo(() => parseSpecs(inputJSON), [inputJSON]);
  const tableName = spec.data && (spec.data as { name: string }).name || 'output';
  const data = results[tableName] || [];


  const resize = React.useCallback(() => {
    // Dispatch a window resize event to signal the chart to redraw. As suggested in:
    // https://vega.github.io/vega-lite/docs/size.html#specifying-responsive-width-and-height
    window.dispatchEvent(new Event('resize'));
  }, []);

  return (
    <AutoSizer onResize={resize}>
      {({ height, width }) => (
        // Grid placeholder
        <div style={{ width, height }}>
          <Vega data={data} spec={spec} tableName={tableName} />
        </div>
      )}
    </AutoSizer >
  );
};

export default Canvas;
