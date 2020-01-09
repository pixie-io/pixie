import {shallow} from 'enzyme';
import * as React from 'react';
import {Collapse} from 'react-bootstrap';

import {Accordion, AccordionToggle} from './accordion';

const testAccordionItems = [
  {
    name: 'item1',
    key: 'item1',
    children: [
      {
        name: 'content1',
        onClick: jest.fn(),
      },
      {
        name: 'content2',
        onClick: jest.fn(),
      },
    ],
  },
  {
    name: 'item2',
    key: 'item2',
    children: [
      {
        name: 'content2-1',
        onClick: jest.fn(),
      },
    ],
  },
];

describe('<Accordion/> test', () => {
  it('renders', () => {
    const wrapper = shallow(<Accordion items={testAccordionItems} />);
    expect(wrapper.find(AccordionToggle)).toHaveLength(2);
    expect(wrapper.find(Collapse)).toHaveLength(2);
  });

  it('default to open the first item', () => {
    const wrapper = shallow(<Accordion items={testAccordionItems} />);
    const activeToggle = wrapper.findWhere((node) => node.type() === AccordionToggle && node.prop('active'));
    expect(activeToggle.length).toBe(1);
    expect(activeToggle.prop('name')).toBe('item1');
    const activeCollapse = wrapper.findWhere((node) => node.type() === Collapse && node.prop('in'));
    expect(activeCollapse.length).toBe(1);
    expect(activeCollapse.key()).toBe('collapse-item1');
  });

  it('closes the active menu when clicked', () => {
    const wrapper = shallow(<Accordion items={testAccordionItems} />);
    const toggle = () => wrapper.findWhere((node) => node.key() === 'toggle-item1');
    expect(toggle().prop('active')).toBe(true);
    const collapse = () => wrapper.findWhere((node) => node.key() === 'collapse-item1');
    expect(collapse().prop('in')).toBe(true);
    toggle().simulate('click');
    expect(toggle().prop('active')).toBe(false);
    expect(collapse().prop('in')).toBe(false);
  });

  it('has at most 1 menu open at a time', () => {
    const wrapper = shallow(<Accordion items={testAccordionItems} />);
    const findByKey = (key) => wrapper.findWhere((node) => node.key() === key);
    expect(findByKey('collapse-item1').prop('in')).toBe(true);
    expect(findByKey('collapse-item2').prop('in')).toBe(false);
    findByKey('toggle-item2').simulate('click');
    expect(findByKey('collapse-item1').prop('in')).toBe(false);
    expect(findByKey('collapse-item2').prop('in')).toBe(true);
  });
});
