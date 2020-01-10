import {AccordionList} from 'components/accordion';
import {mount, shallow} from 'enzyme';
import * as React from 'react';

import {HistoryList} from './script-history';

const testHistoryProps = {
  history: [
    {
      id: 'item1',
      title: 'title 1',
      code: 'some fancy code',
      time: new Date(),
    },
    {
      id: 'item2',
      title: 'unique title',
      code: 'unique code',
      time: new Date(),
    },
  ],
  onClick: jest.fn(),
};

describe('<HistoryList/> test', () => {
  it('renders', () => {
    const wrapper = shallow(<HistoryList {...testHistoryProps} />);
    expect(wrapper.find(AccordionList)).toHaveLength(1);
    expect(wrapper.find('.pixie-history-list-search-input')).toHaveLength(1);
  });

  it('filters the list', () => {
    const wrapper = mount(<HistoryList {...testHistoryProps} />);
    expect(wrapper.find(AccordionList).prop('items')).toHaveLength(2);
    wrapper.find('input').simulate('change', { target: { value: '1' } });
    const filtered = wrapper.find(AccordionList).prop('items');
    expect(filtered).toHaveLength(1);
  });

  it('supports filter by regexp', () => {
    const wrapper = mount(<HistoryList {...testHistoryProps} />);
    expect(wrapper.find(AccordionList).prop('items')).toHaveLength(2);
    wrapper.find('input').simulate('change', { target: { value: '^title' } });
    const filtered = wrapper.find(AccordionList).prop('items');
    expect(filtered).toHaveLength(1);
  });

  it('clears the filter when the clear icon is clicked', () => {
    const wrapper = mount(<HistoryList {...testHistoryProps} />);
    wrapper.find('input').simulate('change', { target: { value: '1' } });
    wrapper.find('.pixie-history-list-search-clear').simulate('click');
    expect(wrapper.find('input').prop('value')).toBe('');
    expect(wrapper.find(AccordionList).prop('items')).toHaveLength(2);
  });

  it('hides the clear icon if nothing is in the input box', () => {
    const wrapper = mount(<HistoryList {...testHistoryProps} />);
    expect(wrapper.find('.pixie-history-list-search-clear')).toHaveLength(0);
    expect(wrapper.find('.pixie-history-list-search-clear-hidden')).toHaveLength(1);
  });
});
