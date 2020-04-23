import * as React from 'react';
import {VisualizationSpec} from 'vega-embed';
import {View} from 'vega-typings';

import {createStyles, makeStyles, Theme, useTheme} from '@material-ui/core/styles';

import {addTooltipsToSpec,  COLOR_SCALE, hydrateSpec, hydrateSpecOld} from './spec';
import {ColoredTooltipHandler} from './tooltip';

const useStyles = makeStyles((theme: Theme) => {
  return createStyles({
    vega: {
      height: '100%',
      width: '100%',
    },
  });
});

interface VegaProps {
  data: Array<{}>;
  spec: VisualizationSpec;
  tableName: string;
  vegaModule: any;
  vegaLiteModule: any;
  oldSpec: boolean;
}

const Vega = React.memo((props: VegaProps) => {
  const classes = useStyles();
  const { data: inputData, spec: inputSpec, tableName, oldSpec } = props;
  const theme = useTheme();
  const spec = React.useMemo(() => {
    if (oldSpec) {
      return hydrateSpecOld(inputSpec, theme, tableName);
    } else {
      return hydrateSpec(inputSpec, theme);
    }
  }, [inputSpec, theme, tableName]);
  const data = React.useMemo(() => ({ [tableName]: inputData }), [tableName, inputData]);

  let vegaSpec = props.vegaLiteModule.compile(spec).spec;
  vegaSpec = addTooltipsToSpec(vegaSpec, spec);

  const onNewView = (view: View) => {
    const handler = new ColoredTooltipHandler(view);
    view.tooltip(handler.call);
  };

  return (
    <props.vegaModule.Vega
      className={classes.vega}
      spec={vegaSpec}
      data={data}
      actions={false}
      onNewView={onNewView}
    />
  );
});

export default Vega;
