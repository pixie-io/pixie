import {SidebarNav} from 'components/sidebar-nav/sidebar-nav';
import {shallow} from 'enzyme';
import * as React from 'react';
import { BrowserRouter as Router, Route } from 'react-router-dom';
import {Vizier} from './vizier';

describe('<Vizier/> test', () => {
  it('should have sidebar', () => {
    const app = shallow(<Vizier
      match=''
      location={ { pathname: 'query' } }
    />);

    expect(app.find(SidebarNav)).toHaveLength(1);
  });
});
