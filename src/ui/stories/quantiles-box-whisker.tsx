import * as React from 'react';

import { useTheme } from '@material-ui/core/styles';

import { QuantilesBoxWhisker, SelectedPercentile } from 'pixie-components';
import { getColor } from 'utils/metric-thresholds';

export default {
  title: 'QuantilesBoxWhisker',
  component: QuantilesBoxWhisker,
};

export const Basic = () => {
  const [selectedPercentile, setSelectedPercentile] = React.useState<SelectedPercentile>('p99');

  return (
    <div style={{ background: 'black' }}>
      <QuantilesBoxWhisker
        p50={100.123}
        p90={200.234}
        p99={500.789}
        max={600}
        p50HoverFill={getColor('low', useTheme())}
        p90HoverFill={getColor('med', useTheme())}
        p99HoverFill={getColor('high', useTheme())}
        p50Display='100 ms'
        p90Display='200 ms'
        p99Display='500 ms'
        selectedPercentile={selectedPercentile}
        onChangePercentile={setSelectedPercentile}
      />
    </div>
  );
};
