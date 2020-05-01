import * as React from 'react';
import { VisualizationSpec } from 'vega-embed';
import { View } from 'vega-typings';

import { ColoredTooltipHandler } from './tooltip';

interface VegaProps {
  data: Array<{}>;
  spec: VisualizationSpec;
  tableName: string;
  reactVegaModule: any;
  className?: string;
}

const Vega = React.memo((props: VegaProps) => {
  const { data: inputData, spec: inputSpec, tableName } = props;
  const data = React.useMemo(() => ({ [tableName]: inputData }), [tableName, inputData]);

  const onNewView = (view: View) => {
    const handler = new ColoredTooltipHandler(view);
    view.tooltip(handler.call);
  };

  return (
    <props.reactVegaModule.Vega
      className={props.className}
      spec={inputSpec}
      data={data}
      actions={false}
      onNewView={onNewView}
    />
  );
});

export default Vega;
