import * as React from 'react';
import {VegaLite} from 'react-vega';
import {AutoSizer} from 'react-virtualized';
import {VisualizationSpec} from 'vega-embed';

import {Theme, useTheme} from '@material-ui/core/styles';

import {ResultsContext, VegaContext} from './context';

function specsFromTheme(theme: Theme) {
  return {
    background: theme.palette.background.default,
  };
}

function parseSpecs(spec: string): VisualizationSpec {
  try {
    const vega = JSON.parse(spec);
    return vega as VisualizationSpec;
  } catch (e) {
    return {};
  }
}

const BASE_SPECS = {
  $schema: 'https://vega.github.io/schema/vega-lite/v4.json',
  width: 'container',
  height: 'container',
};

interface CanvasProps {
  data?: any;
}

const Canvas = (props: CanvasProps) => {
  const theme = useTheme();
  const inputJSON = React.useContext(VegaContext);
  const results = React.useContext(ResultsContext);

  const spec = React.useMemo(() => {
    const inputSpec = parseSpecs(inputJSON);
    return {
      ...inputSpec,
      ...BASE_SPECS,
      ...specsFromTheme(theme),
    } as VisualizationSpec;
  }, [inputJSON, results]);

  const resize = React.useCallback(() => {
    // Dispatch a window resize event to signal the chart to redraw. As suggested in:
    // https://vega.github.io/vega-lite/docs/size.html#specifying-responsive-width-and-height
    window.dispatchEvent(new Event('resize'));
  }, []);

  return (
    <AutoSizer onResize={resize}>
      {({ height, width }) => (
        <VegaLite
          style={{ height, width }}
          spec={spec}
          actions={false}
        />
      )}
    </AutoSizer>
  );
};

export default Canvas;
