import * as React from 'react';

import { storiesOf } from '@storybook/react';
import { StatusCell } from 'components/status/status';
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
          [
            { value: 'cluster1', icon: <StatusCell statusGroup='healthy' /> },
            { value: 'cluster2', icon: <StatusCell statusGroup='unhealthy' /> },
            { value: 'cluster3', icon: <StatusCell statusGroup='pending' /> },
          ]
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
            return [{ value: 'pod1' }, { value: 'pod2' }];
          }
          return [{ value: 'some pod' }, { value: 'another pod' }, { value: 'pod' }];
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
