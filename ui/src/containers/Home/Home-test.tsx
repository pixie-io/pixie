import {shallow} from 'enzyme';
import * as React from 'react';
import {Home} from './Home';

describe('<Home/> test', () => {
  it('should have correct property name', () => {
    const wrapper = shallow(<Home/>);
    expect(wrapper.find('#login-link')).toHaveLength(1);
    expect(wrapper.find('#login-link').text()).toEqual('Login');
  });
});
