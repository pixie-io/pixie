import { shallow } from 'enzyme';
import * as React from 'react';

import { VizierErrorDetails, VizierQueryError } from './errors';

describe('<VizierErrorDetails/> test', () => {
  it('renders the details if it is a VizierQueryError', () => {
    const wrapper = shallow(
      <VizierErrorDetails error={
        new VizierQueryError('server', 'a well formated server error')
      }
      />,
    );
    expect(wrapper.find('div').at(0).text()).toBe('a well formated server error');
  });

  it('renders a list of errors if the details is a list', () => {
    const wrapper = shallow(
      <VizierErrorDetails error={
        new VizierQueryError('script', ['error 1', 'error 2', 'error 3'])
      }
      />,
    );
    expect(wrapper.find('div').length).toBe(4);
  });

  it('renders the message for other errors', () => {
    const wrapper = shallow(
      <VizierErrorDetails error={
        new Error('generic error')
      }
      />,
    );
    expect(wrapper.find('div').at(0).text()).toBe('generic error');
  });
});
