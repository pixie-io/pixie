import { shallow } from 'enzyme';
import * as React from 'react';

import { LazyPanel } from './lazy-panel';

describe('<LazyPanel/> test', () => {
  it('renders null when show is false', () => {
    const wrapper = shallow(
      <LazyPanel show={false}>
        <div className='content'>test content</div>
      </LazyPanel>,
    );

    expect(wrapper.getElement()).toBe(null);
    expect(wrapper.find('.visible')).toHaveLength(0);
  });

  it('renders content when show is true', () => {
    const wrapper = shallow(
      <LazyPanel show>
        <div className='content'>test content</div>
      </LazyPanel>,
    );

    expect(wrapper.find('.content')).toHaveLength(1);
    expect(wrapper.find('.visible')).toHaveLength(1);
  });

  it("doesn't destroy the element if show becomes false", () => {
    const wrapper = shallow(
      <LazyPanel show>
        <div className='content'>test content</div>
      </LazyPanel>,
    );

    wrapper.setProps({ show: false });
    expect(wrapper.find('.content')).toHaveLength(1);
    expect(wrapper.find('.visible')).toHaveLength(0);
  });
});
