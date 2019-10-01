import {shallow} from 'enzyme';
import * as React from 'react';
import {Dropdown} from 'react-bootstrap';
import { Link } from 'react-router-dom';
import {SidebarItem, SidebarMenuItem, SidebarNav} from './sidebar-nav';

describe('<SidebarNav/> test', () => {
  it('should have correct icons', () => {
    const wrapper = shallow(<SidebarNav
      logo={'testLogo'}
      items={[
        { link: '/', selectedImg: 'selectedImg1', unselectedImg: 'unselectedImg1'},
        { link: '/2', selectedImg: 'selectedImg2', unselectedImg: 'unselectedImg2'},
        { link: '/3', selectedImg: 'selectedImg3', unselectedImg: 'unselectedImg3'},

      ]}
      footerItems={[
        { link: '/4', selectedImg: 'selectedImg4', unselectedImg: 'unselectedImg4'},

      ]}
    />);
    expect(wrapper.find('.sidebar-nav--logo img').prop('src')).toEqual('testLogo');
    expect(wrapper.find(SidebarItem)).toHaveLength(4);
    expect(wrapper.find(SidebarItem).at(0).prop('unselectedImg')).toEqual('unselectedImg1');
    expect(wrapper.find(SidebarItem).at(1).prop('unselectedImg')).toEqual('unselectedImg2');
    expect(wrapper.find(SidebarItem).at(2).prop('unselectedImg')).toEqual('unselectedImg3');
    expect(wrapper.find(SidebarItem).at(3).prop('unselectedImg')).toEqual('unselectedImg4');
  });
});

describe('<SidebarItem/> test', () => {
  it('should show redirect links', () => {
    const wrapper = shallow(<SidebarItem
      link='/'
      redirect={true}
      selectedImg='selectedImg'
      unselectedImg='unselectedImg'
    />);
    expect(wrapper.find('.sidebar-nav--link')).toHaveLength(1);
    expect(wrapper.find(SidebarMenuItem)).toHaveLength(0);
  });

  it('should show react links', () => {
    const wrapper = shallow(<SidebarItem
      link='/abcd'
      selectedImg='selectedImg'
      unselectedImg='unselectedImg'
    />);
    expect(wrapper.find(Link)).toHaveLength(1);
    expect(wrapper.find(Link).at(0).prop('to')).toEqual('/abcd');
    expect(wrapper.find(SidebarMenuItem)).toHaveLength(0);
  });

  it('should show menu item', () => {
    const wrapper = shallow(<SidebarItem
      menu={{a: '/test', b: '/abcd'}}
      selectedImg='selectedImg'
      unselectedImg='unselectedImg'
    />);
    expect(wrapper.find('sidebar-nav--link')).toHaveLength(0);
    expect(wrapper.find(SidebarMenuItem)).toHaveLength(1);
    expect(wrapper.find(SidebarMenuItem).at(0).prop('unselectedImg')).toEqual('unselectedImg');
    expect(wrapper.find(SidebarMenuItem).at(0).prop('selectedImg')).toEqual('selectedImg');
    expect(wrapper.find(SidebarMenuItem).at(0).prop('menu')).toEqual({a: '/test', b: '/abcd'});
  });
});

describe('<SidebarMenuItem/> test', () => {
  it('render dropdown correctly', () => {
    const wrapper = shallow(<SidebarMenuItem
      menu={{a: '/test', b: '/abcd'}}
      selectedImg='selectedImg'
      unselectedImg='unselectedImg'
    />);

    expect(wrapper.find(Dropdown)).toHaveLength(1);
    expect(wrapper.find('img')).toHaveLength(1);
    expect(wrapper.find('img').at(0).prop('src')).toEqual('unselectedImg');
    expect(wrapper.find(Dropdown.Item)).toHaveLength(2);
    expect(wrapper.find(Dropdown.Item).at(0).prop('id')).toEqual('a');
    expect(wrapper.find(Dropdown.Item).at(1).prop('id')).toEqual('b');
  });
});
