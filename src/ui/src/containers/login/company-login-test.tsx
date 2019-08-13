import {mount} from 'enzyme';
import * as React from 'react';
import {Button, InputGroup} from 'react-bootstrap';
import { BrowserRouter as Router, Route } from 'react-router-dom';
import {CompanyCreate, CompanyLogin} from './company-login';

describe('<CompanyCreate/> test', () => {
  it('should have correct content', () => {
    const app = mount(<Router><CompanyCreate/></Router>);

    expect(app.find('h3').at(0).text()).toEqual('Claim your site');
    expect(app.find(InputGroup)).toHaveLength(1);
    expect(app.find(Button)).toHaveLength(1);
    expect(app.find(
      '.company-login-content--footer-text').at(0).text())
      .toEqual('Already have a site? Click here to log in');
  });
});

describe('<CompanyLogin/> test', () => {
  it('should have correct content', () => {
    const app = mount(<Router><CompanyLogin/></Router>);

    expect(app.find('h3').at(0).text()).toEqual('Log in to your company');
    expect(app.find(InputGroup)).toHaveLength(1);
    expect(app.find(Button)).toHaveLength(1);
    expect(app.find(
      '.company-login-content--footer-text').at(0).text())
      .toEqual('Don\'t have a company site yet? Claim your site here');
  });
});
