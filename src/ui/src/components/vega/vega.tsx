import * as React from 'react';
import {VegaLite} from 'react-vega';
import {VisualizationSpec} from 'vega-embed';

import {createStyles, makeStyles, Theme, useTheme} from '@material-ui/core/styles';

import {hydrateSpec} from './spec';

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
}

const Vega = React.memo((props: VegaProps) => {
  const classes = useStyles();
  const { data: inputData, spec: inputSpec, tableName } = props;
  const theme = useTheme();
  const spec = React.useMemo(() => hydrateSpec(inputSpec, theme, tableName), [inputSpec, theme, tableName]);
  const data = React.useMemo(() => ({ [tableName]: inputData }), [tableName, inputData]);

  return (
    <VegaLite
      className={classes.vega}
      spec={spec}
      data={data}
      actions={false}
    />
  );
});

export default Vega;
