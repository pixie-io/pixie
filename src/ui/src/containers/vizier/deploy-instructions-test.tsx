import {CodeSnippet} from 'components/code-snippet/code-snippet';
import {DialogBox} from 'components/dialog-box/dialog-box';
import {mount, shallow} from 'enzyme';
import * as React from 'react';
import {MockedProvider} from 'react-apollo/test-utils';

import {DeployInstructions} from './deploy-instructions';

const wait = (ms) => new Promise((res) => setTimeout(res, ms));

describe('<DeployInstructionsContent/> test', () => {
  it('should show correct content', () => {

    const wrapper = shallow(
      <DeployInstructions/>);

    expect(wrapper.find(CodeSnippet)).toHaveLength(2);
  });
});
