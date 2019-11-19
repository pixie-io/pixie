import Axios from 'axios';
import MockAdapter from 'axios-mock-adapter';
import {mount} from 'enzyme';
import * as React from 'react';
import {Button, InputGroup} from 'react-bootstrap';
import {BrowserRouter as Router, Route} from 'react-router-dom';

import {CompanyCreate, CompanyLogin} from './company-login';

jest.mock('containers/constants', () => ({ DOMAIN_NAME: 'dev.withpixie.dev' }));
// Mock out window.location because jsdom doesn't handle redirects.
const windowLocation = JSON.stringify(window.location);
delete window.location;
Object.defineProperty(window, 'location', {
  value: JSON.parse(windowLocation),
});

describe.skip('<CompanyCreate/> test', () => {
  it('should have correct content', () => {
    const app = mount(<Router><CompanyCreate /></Router>);

    expect(app.find('h3').at(0).text()).toEqual('Claim your site');
    expect(app.find(InputGroup)).toHaveLength(1);
    expect(app.find(Button)).toHaveLength(1);
    expect(app.find(
      '.company-login-content--footer-text').at(0).text())
      .toEqual('Already have a site? Click here to log in');

    expect(app.find(InputGroup.Text).at(0).text()).toEqual('.dev.withpixie.dev');
  });

  it('should show available', (done) => {
    const mock = new MockAdapter(Axios);

    mock.onGet('/api/site/check').reply(200, {
      available: true,
    });
    const app = mount(<Router><CompanyCreate /></Router>);

    const button = app.find(Button);
    expect(button.get(0).props.disabled).toBe(false);
    button.at(0).simulate('click');

    setImmediate(() => {
      app.update();
      expect(app.find('.company-login-content--error')
        .at(0).text()).toEqual('');
      expect(app.find(Button).get(0).props.disabled).toBe(false);
      done();
    });
  });

  it('should show error', (done) => {
    const mock = new MockAdapter(Axios);

    mock.onGet('/api/site/check').reply(200, {
      available: false,
    });
    const app = mount(<Router><CompanyCreate /></Router>);

    const button = app.find(Button);
    expect(button.get(0).props.disabled).toBe(false);
    button.at(0).simulate('click');

    setImmediate(() => {
      app.update();
      expect(app.find('.company-login-content--error')
        .at(0).text()).toEqual('Sorry, the site already exists. Try a different name.');
      done();
    });
  });

  it('should disable the submit button when requests are inflight', (done) => {
    const mock = new MockAdapter(Axios);

    let resolve: () => void;
    mock.onGet('/api/site/check').reply(() => new Promise((res, rej) => {
      resolve = () => {
        res([200, { available: false }]);
      };
    }));
    const app = mount(<Router><CompanyCreate /></Router>);

    const button = app.find(Button);
    expect(button.get(0).props.disabled).toBe(false);
    button.at(0).simulate('click');

    setImmediate(() => {
      app.update();
      expect(app.find('.company-login-content--submit').first().prop('disabled')).toBe(true);
      expect(app.find('.company-login-content--input').first().prop('disabled')).toBe(true);
      resolve();
      done();
    });
  });
});

describe.skip('<CompanyLogin/> test', () => {
  it('should have correct content', () => {
    const app = mount(<Router><CompanyLogin /></Router>);

    expect(app.find('h3').at(0).text()).toEqual('Log in to your company');
    expect(app.find(InputGroup)).toHaveLength(1);
    expect(app.find(Button)).toHaveLength(1);
    expect(app.find(
      '.company-login-content--footer-text').at(0).text())
      .toEqual('Don\'t have a company site yet? Claim your site here');
    expect(app.find(InputGroup.Text).at(0).text()).toEqual('.dev.withpixie.dev');
  });

  it('should show error', (done) => {
    const mock = new MockAdapter(Axios);

    mock.onGet('/api/site/check').reply(200, {
      available: true,
    });
    const app = mount(<Router><CompanyLogin /></Router>);

    const button = app.find(Button);
    expect(button.get(0).props.disabled).toBe(false);
    button.at(0).simulate('click');

    setImmediate(() => {
      app.update();
      expect(app.find('.company-login-content--error')
        .at(0).text()).toEqual('The site doesn\'t exist. Please check the name and try again.');
      done();
    });
  });

  it('should allow login', (done) => {
    const mock = new MockAdapter(Axios);

    mock.onGet('/api/site/check').reply(200, {
      available: false,
    });
    const app = mount(<Router><CompanyLogin /></Router>);

    const button = app.find(Button);
    expect(button.get(0).props.disabled).toBe(false);
    button.at(0).simulate('click');

    setImmediate(() => {
      app.update();
      expect(app.find('.company-login-content--error')
        .at(0).text()).toEqual('');
      expect(app.find(Button).get(0).props.disabled).toBe(false);
      done();
    });
  });

  it('should disable the submit button when requests are inflight', (done) => {
    const mock = new MockAdapter(Axios);

    let resolve: () => void;
    mock.onGet('/api/site/check').reply(() => new Promise((res, rej) => {
      resolve = () => {
        res([200, { available: false }]);
      };
    }));
    const app = mount(<Router><CompanyLogin /></Router>);

    const button = app.find(Button);
    expect(button.get(0).props.disabled).toBe(false);
    button.at(0).simulate('click');

    setImmediate(() => {
      app.update();
      expect(app.find('.company-login-content--submit').first().prop('disabled')).toBe(true);
      expect(app.find('.company-login-content--input').first().prop('disabled')).toBe(true);
      resolve();
      done();
    });
  });
});
