import { shallow } from 'enzyme';
import * as React from 'react';
import { Route } from 'react-router-dom';

import { Login } from './login';

describe('<Login/> test', () => {
  it('should have correct routes', () => {
    const app = shallow(<Login />);

    expect(app.find(Route)).toHaveLength(3);
  });
});
