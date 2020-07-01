import * as React from 'react';
import { render } from 'enzyme';
import VersionInfo from './version-info';

describe('<VersionInfo/>', () => {
  it('renders correctly', () => {
    const wrapper = render(<VersionInfo cloudVersion='testing 123' />);
    expect(wrapper).toMatchSnapshot();
  });
});
