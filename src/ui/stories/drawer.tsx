import * as React from 'react';

import { storiesOf } from '@storybook/react';

import { Drawer } from '../src/components/drawer/drawer';

storiesOf('Drawer', module)
  .add('Basic', () => (
    <div style={{ margin: '20px', display: 'flex', height: '400px' }}>
      <Drawer>drawer content</Drawer>
      <div style={{ background: 'white', flex: 1 }}>main content</div>
    </div>
  ), {
    info: { inline: true },
    notes: 'A simple collapsible drawer',
  })
  .add('Default closed', () => (
    <div style={{ margin: '20px', display: 'flex', height: '400px' }}>
      <Drawer defaultOpened={false}>drawer content</Drawer>
      <div style={{ background: 'white', flex: 1 }}>main content</div>
    </div>
  ), {
    info: { inline: true },
    notes: 'The drawer defaults to be opened. Set the prop defaultOpened to '
      + 'false to make it closed',
  })
  .add('Custom Width', () => (
    <div style={{ margin: '20px', display: 'flex', height: '400px' }}>
      <Drawer
        closedWidth='60px'
        openedWidth='300px'
      >
        drawer content
      </Drawer>
      <div style={{ background: 'white', flex: 1 }}>main content</div>
    </div>
  ), {
    info: { inline: true },
    notes: 'A simple collapsible drawer',
  });
