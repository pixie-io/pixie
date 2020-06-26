import { shallow } from 'enzyme';
import * as React from 'react';
import { Header } from './header';

describe('<Header/> test', () => {
  it('should have correct header text', () => {
    const wrapper = shallow(<Header
      primaryHeading={'primary heading'}
      secondaryHeading={'secondary heading'}
    />);
    expect(wrapper.find('.header--text').text()).toEqual('primary heading|secondary heading');
  });
});
