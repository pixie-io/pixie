import { shallow } from 'enzyme';
import * as React from 'react';

import { CommandAutocompleteInput } from 'components/autocomplete/command-autocomplete-input';

const noop = () => {};

describe('<AutcompleteInput/> test', () => {
  it('renders the correct spans', () => {
    const wrapper = shallow(
      <CommandAutocompleteInput
        onKey={noop}
        onChange={noop}
        setCursor={noop}
        cursorPos={8}
        placeholder='test'
        isValid={false}
        value={[
          {
            type: 'key',
            value: 'svc: ',
          },
          {
            type: 'value',
            value: 'pl/test',
          },
        ]}
      />
    );
    expect(wrapper.find('span')).toHaveLength(4);
    expect(wrapper.find('span').at(0).text()).toBe('svc: ');
    expect(wrapper.find('span').at(1).text()).toBe('pl/');
    expect(wrapper.find('span').at(2).text()).toBe('test');
    expect(wrapper.find('span').at(3).text()).toBe(''); // Placeholder span.
  });

  it('renders placeholder', () => {
    const wrapper = shallow(
      <CommandAutocompleteInput
        onKey={noop}
        onChange={noop}
        setCursor={noop}
        cursorPos={0}
        placeholder='test'
        value={[]}
        isValid={false}
      />
    );
    expect(wrapper.find('span')).toHaveLength(1);
    expect(wrapper.find('span').at(0).text()).toBe('test'); // Placeholder span.
  });
});
