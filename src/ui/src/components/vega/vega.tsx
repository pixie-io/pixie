import * as React from 'react';
import {VisualizationSpec} from 'vega-embed';

import {createStyles, makeStyles, Theme, useTheme} from '@material-ui/core/styles';

import {hydrateSpec, hydrateSpecOld} from './spec';

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

  return (
    <props.vegaModule.VegaLite
      className={classes.vega}
      spec={spec}
      data={data}
      actions={false}
    />
  );
});

export default Vega;
