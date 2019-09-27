import {shallow} from 'enzyme';
import * as React from 'react';
import { BrowserRouter as Router, Route } from 'react-router-dom';
import {Login} from './login';

describe('<Login/> test', () => {
  it('should have correct routes', () => {
    const app = shallow(<Login match=''/>);

    expect(app.find(Route)).toHaveLength(5);
  });
});
