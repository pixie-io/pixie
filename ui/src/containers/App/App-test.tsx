import {shallow} from 'enzyme';
import * as React from 'react';
import {App} from './App';

describe('<App/> test', () => {
  it('should set name based on property', () => {
    const app = shallow(<App name='test message' />);
    expect(app.text()).toEqual('test message');
  });
});
