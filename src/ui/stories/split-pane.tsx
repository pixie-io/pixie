import { SplitContainer, SplitPane } from 'components/split-pane/split-pane';
import * as React from 'react';

import { storiesOf } from '@storybook/react';

storiesOf('SplitPane', module)
  .add('Basic', () => (
    <div style={{ height: '400px' }}>
      <SplitContainer>
        <SplitPane title='pane1' id='1'>
          <div style={{ backgroundColor: 'black', height: '100%' }}>content1</div>
        </SplitPane>
        <SplitPane title='pane2' id='2'>
          <div style={{ backgroundColor: 'black', height: '100%' }}>content2</div>
        </SplitPane>
      </SplitContainer>
    </div>
  ), {
    info: { inline: true },
    notes: 'Split pane that support resizing of the panes and collapsing panes by click the pane header.',
  });
