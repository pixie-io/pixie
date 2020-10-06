import * as React from 'react';

import { storiesOf } from '@storybook/react';
import QuantilesBoxWhisker, {
  SelectedPercentile,
} from '../src/components/quantiles-box-whisker/quantiles-box-whisker';

storiesOf('QuantilesBoxWhisker', module)
  .add('Quantiles box whisker', () => {
    const [selectedPercentile, setSelectedPercentile] = React.useState<SelectedPercentile>('p99');

    return (
      <div style={{ background: 'black' }}>
        <QuantilesBoxWhisker
          p50={100.123}
          p90={200.234}
          p99={500.789}
          max={600}
          p50Level='low'
          p90Level='med'
          p99Level='high'
          p50Display='100 ms'
          p90Display='200 ms'
          p99Display='500 ms'
          selectedPercentile={selectedPercentile}
          onChangePercentile={setSelectedPercentile}
        />
      </div>
    );
  }, {
    info: { inline: true },
    notes: 'This is a quantiles box whisker plot.',
  });
