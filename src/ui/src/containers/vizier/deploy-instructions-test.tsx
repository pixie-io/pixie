import {CodeSnippet} from 'components/code-snippet/code-snippet';
import {DialogBox} from 'components/dialog-box/dialog-box';
import {shallow} from 'enzyme';
import * as React from 'react';
import {DeployInstructions} from './deploy-instructions';

describe('<DeployInstructions/> test', () => {
  it('should show correct content', () => {
    const wrapper = shallow(<DeployInstructions
      sitename={'pixielabs'}
      clusterID={'test'}
    />);
    expect(wrapper.find('.deploy-instructions--subheader').text()).toEqual('pixielabs');
    expect(wrapper.find(DialogBox)).toHaveLength(1);
    expect(wrapper.find(CodeSnippet)).toHaveLength(1);
  });
});
