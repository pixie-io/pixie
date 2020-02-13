import {shallow} from 'enzyme';
import * as React from 'react';
import {noop} from 'utils/testing';

import Completions, {Completion} from './completions';

jest.mock('clsx', () => ({ default: jest.fn() }));

describe('<Completions/> test', () => {
  it('renders', () => {
    const wrapper = shallow(
      <Completions
        inputValue='script'
        items={[
          { header: 'Recently used' },
          { title: 'px/script1', id: 'px-0', highlights: [[3, 5]] },
          { title: 'px/script2', id: 'px-1', highlights: [[3, 5]] },
          { title: 'px/script3', id: 'px-2', highlights: [[3, 5]] },
          { header: 'Org scripts' },
          { title: 'hulu/script1', id: 'hulu-4' },
          { title: 'hulu/script2', id: 'hulu-5' },
        ]}
        onActiveChange={noop}
        activeItem='px-1'
        onSelection={noop}
      />,
    );
    expect(wrapper.find(Completion)).toHaveLength(5);
    expect(wrapper.findWhere((node) => node.prop('active')).prop('id')).toBe('px-1');
  });
});

describe('<Completion> test', () => {
  describe('renders highlights', () => {
    it('from the beginning', () => {
      const wrapper = shallow(
        <Completion
          id='some id'
          title='0123456789'
          highlights={[[0, 4]]}
          active={false}
          onActiveChange={noop}
          onSelection={noop}
        />,
      );
      expect(wrapper.find('span')).toHaveLength(2);
      expect(wrapper.find('span').first().text()).toBe('0123');
    });

    it('in the middle', () => {
      const wrapper = shallow(
        <Completion
          id='some id'
          title='0123456789'
          highlights={[[1, 2]]}
          active={false}
          onActiveChange={noop}
          onSelection={noop}
        />,
      );
      expect(wrapper.find('span')).toHaveLength(3);
      expect(wrapper.find('span').at(0).text()).toBe('0');
      expect(wrapper.find('span').at(1).text()).toBe('1');
      expect(wrapper.find('span').at(2).text()).toBe('23456789');
    });
  });
});
