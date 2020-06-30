import Axios from 'axios';
import MockAdapter from 'axios-mock-adapter';
import { shallow } from 'enzyme';
import * as React from 'react';
import { Logout } from './logout';

describe('<Logout/> test', () => {
  it('should logout', () => {
    const mock = new MockAdapter(Axios);

    mock.onPost('/api/logout').reply(200);
    const app = shallow(<Logout />);

    expect(app.find('.login').text()).toEqual('Logging out...');
  });
});
