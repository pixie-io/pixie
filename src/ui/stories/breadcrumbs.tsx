import * as React from 'react';

import { storiesOf } from '@storybook/react';
import Breadcrumbs from '../src/components/breadcrumbs/breadcrumbs';

storiesOf('Breadcrumbs', module)
  .add('basic breadcrumbs', () => {
    const breadcrumbs = [
      {
        title: 'cluster',
        value: 'gke-prod',
        selectable: true,
        allowTyping: false,
        // eslint-disable-next-line
        getListItems: async (input) => (
          ['cluster1', 'cluster2', 'cluster3']
        ),
        onSelect: (input) => {
          // eslint-disable-next-line
          console.log(`Selected cluster: ${input}`);
        },
      },
      {
        title: 'pod',
        value: 'pod-123',
        selectable: true,
        allowTyping: true,
        getListItems: async (input) => {
          if (input === '') {
            return ['pod1', 'pod2'];
          }
          return ['some pod', 'another pod', 'pod'];
        },
        onSelect: (input) => {
          // eslint-disable-next-line
          console.log(`Selected pod: ${input}`);
        },
      },
      {
        title: 'script',
        value: 'px/pod',
        selectable: false,
      },
    ];

    return (
      <div style={{ backgroundColor: '#212324' }}>
        <Breadcrumbs
          breadcrumbs={breadcrumbs}
        />
      </div>
    );
  }, {
    info: { inline: true },
    notes: 'This is the basic breadcrumbs component.',
  });
