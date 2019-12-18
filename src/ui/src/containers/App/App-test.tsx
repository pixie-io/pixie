import {SidebarNav} from 'components/sidebar-nav/sidebar-nav';
import {shallow} from 'enzyme';
import * as React from 'react';
import {BrowserRouter as Router, Route} from 'react-router-dom';

import {App} from './App';

describe('<App/> test', () => {
  it.skip('should have correct routes', () => {
    const app = shallow(<App />);

    expect(app.find(Route)).toHaveLength(3);
  });
});
