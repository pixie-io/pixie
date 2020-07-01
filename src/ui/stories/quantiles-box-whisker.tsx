import * as React from 'react';

import { storiesOf } from '@storybook/react';

import QuantilesBoxWhisker from '../src/components/quantiles-box-whisker/quantiles-box-whisker';

storiesOf('QuantilesBoxWhisker', module)
  .add('Quantiles box whisker', () => (
    <QuantilesBoxWhisker p50={100.123} p90={200.234} p99={500.789} max={600} p99Level='high' />
  ), {
    info: { inline: true },
    notes: 'This is a quantiles box whisker plot.',
  });
