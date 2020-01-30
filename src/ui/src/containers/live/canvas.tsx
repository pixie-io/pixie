import * as React from 'react';
import {VegaLite} from 'react-vega';
import {AutoSizer} from 'react-virtualized';
import {VisualizationSpec} from 'vega-embed';

import {Theme, useTheme} from '@material-ui/core/styles';

function specsFromTheme(theme: Theme) {
  return {
    background: theme.palette.background.default,
  };
}

const BASE_SPECS = {
  $schema: 'https://vega.github.io/schema/vega-lite/v4.json',
  width: 'container',
  height: 'container',
};

interface CanvasProps {
  spec: object;
  data?: any;
}

const Canvas = (props: CanvasProps) => {
  const theme = useTheme();
  const spec = React.useMemo(() => ({
    ...props.spec,
    ...BASE_SPECS,
    ...specsFromTheme(theme),
  } as VisualizationSpec),
    [props.spec]);

  return (
    <AutoSizer>
      {({ height, width }) => (
        // TODO(malthus): Figure out how to force vega to redraw the canvas on resize.
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
