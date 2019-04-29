import {shallow} from 'enzyme';
import * as React from 'react';
import {ContentBox} from './content-box';

describe('<ContentBox/> test', () => {
  it('should correct set header texts', () => {
    const wrapper = shallow(<ContentBox
      headerText='header'
      subheaderText='subheader'
      secondaryText='secondary text'
    >
      {'Here is a child.'}
    </ContentBox>);
    expect(wrapper.find('.content-box--header-text').text()).toEqual('HEADER');
    expect(wrapper.find('.content-box--subheader-text').text()).toEqual('| subheader');
    expect(wrapper.find('.content-box--secondary-text').text()).toEqual('secondary text');
  });

  it('should allow empty sub/secondary headers', () => {
    const wrapper = shallow(<ContentBox
      headerText='header'
    >
      {'Here is a child.'}
    </ContentBox>);
    expect(wrapper.find('.content-box--header-text').text()).toEqual('HEADER');
    expect(wrapper.find('.content-box--subheader-text').text()).toEqual('');
    expect(wrapper.find('.content-box--secondary-text').text()).toEqual('');
  });

  it('should properly nest children', () => {
    const wrapper = shallow(<ContentBox
      headerText='header'
    >
      {'Here is a child.'}
    </ContentBox>);
    expect(wrapper.find('.content-box--content').text()).toEqual('Here is a child.');
  });
});
