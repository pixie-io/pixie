import {shallow} from 'enzyme';
import * as React from 'react';
import {SidebarNav} from './sidebar-nav';

describe('<SidebarNav/> test', () => {
  it('should have correct icons', () => {
    const wrapper = shallow(<SidebarNav
      logo={'testLogo'}
      items={[
        { link: '/', selectedImg: 'selectedImg1', unselectedImg: 'unselectedImg1'},
        { link: '/2', selectedImg: 'selectedImg2', unselectedImg: 'unselectedImg2'},
        { link: '/3', selectedImg: 'selectedImg3', unselectedImg: 'unselectedImg3'},

      ]}
    />);
    expect(wrapper.find('.sidebar-nav--logo img').prop('src')).toEqual('testLogo');
    expect(wrapper.find('.sidebar-nav--item')).toHaveLength(3);
    expect(wrapper.find('.sidebar-nav--item').at(0).find('img').prop('src')).toEqual('selectedImg1');
    expect(wrapper.find('.sidebar-nav--item').at(1).find('img').prop('src')).toEqual('unselectedImg2');
    expect(wrapper.find('.sidebar-nav--item').at(2).find('img').prop('src')).toEqual('unselectedImg3');
  });
});
