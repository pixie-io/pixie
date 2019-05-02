import {shallow} from 'enzyme';
import * as React from 'react';
import {Dropdown, DropdownButton} from 'react-bootstrap';
import * as CodeMirror from 'react-codemirror';
import {QueryManager} from './query-manager';

describe('<QueryManager/> test', () => {
  it('should update code upon dropdown selection', () => {
    const wrapper = shallow(<QueryManager/>);
    const dropdown = wrapper.find(DropdownButton).at(0);
    dropdown.simulate('click');

    expect(wrapper.find(Dropdown.Item)).toHaveLength(5);
    const dropdownItem = wrapper.find(Dropdown.Item).at(1);
    dropdownItem.simulate('select', '1');

    expect(wrapper.state('code')).toContain('t1 = From(table=\'bcc_http_trace\',');
  });
});
