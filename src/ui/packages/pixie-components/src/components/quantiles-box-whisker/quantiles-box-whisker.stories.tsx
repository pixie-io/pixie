import * as React from 'react';

import { useTheme } from '@material-ui/core/styles';

import {
  QuantilesBoxWhisker,
  SelectedPercentile,
} from './quantiles-box-whisker';

export default {
  title: 'QuantilesBoxWhisker',
  component: QuantilesBoxWhisker,
};

export const Basic = () => {
  const [selectedPercentile, setSelectedPercentile] = React.useState<
  SelectedPercentile
  >('p99');
  const theme = useTheme();

  return (
    <div style={{ background: 'black' }}>
      <QuantilesBoxWhisker
        p50={100.123}
        p90={200.234}
        p99={500.789}
        max={600}
        p50HoverFill={theme.palette.success.main}
        p90HoverFill={theme.palette.warning.main}
        p99HoverFill={theme.palette.error.main}
        p50Display='100 ms'
        p90Display='200 ms'
        p99Display='500 ms'
        selectedPercentile={selectedPercentile}
        onChangePercentile={setSelectedPercentile}
      />
    </div>
  );
};
