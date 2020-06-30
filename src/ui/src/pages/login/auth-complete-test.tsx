import { shallow } from 'enzyme';
import * as React from 'react';

import { AuthComplete } from './auth-complete';

describe('<AuthSuccess/> test', () => {
  it('should show correct content when no errors', () => {
    const wrapper = shallow(<AuthComplete location={{ search: '' }} />);

    expect(wrapper.find('.pixie-auth-complete-msg').text()).toContain('successful');
  });

  it('should show correct content on token error', () => {
    const wrapper = shallow(<AuthComplete location={{ search: '?err=token' }} />);

    expect(wrapper.find('.pixie-auth-complete-msg').text()).toContain('permission');
  });

  it('should show correct content other errors', () => {
    const wrapper = shallow(<AuthComplete location={{ search: '?err=true' }} />);

    expect(wrapper.find('.pixie-auth-complete-msg').text()).toContain('try again later');
  });
});
