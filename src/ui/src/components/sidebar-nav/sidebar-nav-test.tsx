import {shallow} from 'enzyme';
import * as React from 'react';
import {SidebarItem, SidebarNav} from './sidebar-nav';

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
