import { shallow } from 'enzyme';
import * as React from 'react';
import { DialogBox } from './dialog-box';

describe('<DialogBox/> test', () => {
  it('should properly nest children', () => {
    const wrapper = shallow(
      <DialogBox
        width={150}
      >
        Here is a child.
      </DialogBox>);
    expect(wrapper.find('.dialog-box--content').text()).toEqual('Here is a child.');
  });

  it('should set correct width', () => {
    const wrapper = shallow(
      <DialogBox
        width={500}
      >
        Here is a child.
      </DialogBox>);
    expect(wrapper.find('.dialog-box').get(0).props.style.width).toBe(500);
  });
});
