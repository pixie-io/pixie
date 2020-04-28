import * as React from 'react';
import {VisualizationSpec} from 'vega-embed';
import {View} from 'vega-typings';

import {createStyles, makeStyles, Theme, useTheme} from '@material-ui/core/styles';

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
}

const Vega = React.memo((props: VegaProps) => {
  const classes = useStyles();
  const { data: inputData, spec: inputSpec, tableName} = props;
  const theme = useTheme();
  const data = React.useMemo(() => ({ [tableName]: inputData }), [tableName, inputData]);

  const onNewView = (view: View) => {
    const handler = new ColoredTooltipHandler(view);
    view.tooltip(handler.call);
  };

  return (
    <props.vegaModule.Vega
      className={classes.vega}
      spec={inputSpec}
      data={data}
      actions={false}
      onNewView={onNewView}
    />
  );
});

export default Vega;
