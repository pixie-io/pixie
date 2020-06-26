import { shallow } from 'enzyme';
import * as React from 'react';
import { Alert } from './alert';

describe('<Alert/> test', () => {
  it('should show correct text', () => {
    const wrapper = shallow(<Alert>
      {'Here is a child.'}
    </Alert>);
    expect(wrapper.find('.pl-alert--content').text()).toEqual('Here is a child.');
  });
});
