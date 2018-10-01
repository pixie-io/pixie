import {shallow} from 'enzyme';
import * as React from 'react';
import { BrowserRouter as Router, Route } from 'react-router-dom';
import {Home} from './Home';

describe('<Home/> test', () => {
  it('should have correct property name', () => {
    const wrapper = shallow(<Home
      name='test message'
    />);

    expect(wrapper.text()).toEqual('test messageLog In');
  });
});
