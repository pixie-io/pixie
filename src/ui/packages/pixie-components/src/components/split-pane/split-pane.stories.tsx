import * as React from 'react';

import { SplitContainer, SplitPane } from './split-pane';

export default {
  title: 'SplitContainer',
  component: SplitContainer,
  subcomponents: { SplitPane },
  decorators: [
    (Story) => (
      <div style={{ color: 'white', height: '400px' }}>
        <Story />
      </div>
    ),
  ],
};

export const Basic = () => (
  <SplitContainer>
    <SplitPane title='pane1' id='1'>
      <div style={{ backgroundColor: 'black', height: '100%' }}>content1</div>
    </SplitPane>
    <SplitPane title='pane2' id='2'>
      <div style={{ backgroundColor: 'black', height: '100%' }}>content2</div>
    </SplitPane>
  </SplitContainer>
);
