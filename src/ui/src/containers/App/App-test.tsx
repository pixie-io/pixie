import {SidebarNav} from 'components/sidebar-nav/sidebar-nav';
import {shallow} from 'enzyme';
import * as React from 'react';
import { BrowserRouter as Router, Route } from 'react-router-dom';
import {App} from './App';

describe('<App/> test', () => {
  it('should have correct routes', () => {
    const app = shallow(<App
      name='test message'
    />);

    expect(app.find(Route)).toHaveLength(3);
  });

  it('should have sidebar', () => {
    const app = shallow(<App
      name='test message'
    />);

    expect(app.find(SidebarNav)).toHaveLength(1);
  });

});
