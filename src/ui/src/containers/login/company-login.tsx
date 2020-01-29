import './company-login.scss';

import Axios from 'axios';
import {DialogBox} from 'components/dialog-box/dialog-box';
import {DOMAIN_NAME} from 'containers/constants';
import * as React from 'react';
import {Button, Form, FormControl, InputGroup} from 'react-bootstrap';
import {Link} from 'react-router-dom';
import analytics from 'utils/analytics';
import * as RedirectUtils from 'utils/redirect-utils';

function checkSiteAvailability(siteName: string): Promise<boolean> {
  return Axios({
    method: 'get',
    url: '/api/site/check',
    params: {
      site_name: siteName,
    },
  }).then((response) => {
    analytics.track('Site check success', { siteName, availability: response.data.available });
    return response.data.available;
  }).catch((err) => {
    analytics.track('Site check failed', { error: err });
    throw new Error('Failed to check site name availability. Please try again later.');
  });
}

interface CompanyDialogProps {
  title: string;
  footerText: string;
  footerLink: string;
  footerLinkText: string;
  onSiteCheck: (siteName: string, available: boolean) => void;
}

export const CompanyLogin = () => {
  return (<CompanyDialog
    title='Log in to your company'
    footerText='Don&apos;t have a company site yet?'
    footerLink='/create'
    footerLinkText='Claim your site here'
    onSiteCheck={(siteName, available) => {
      if (available) {
        throw new Error('The site doesn\'t exist. Please check the name and try again.');
      }
      RedirectUtils.redirect('id', '/login', { ['site_name']: siteName });
    }}
  />);
};

export const CompanyCreate = () => {
  return (<CompanyDialog
    title='Claim your site'
    footerText='Already have a site?'
    footerLink='/'
    footerLinkText='Click here to log in'
    onSiteCheck={(siteName, available) => {
      if (!available) {
        throw new Error('Sorry, the site already exists. Try a different name.');
      }
      RedirectUtils.redirect('id', '/create-site', { ['site_name']: siteName });
    }}
  />);
};

interface CompanyDialogState {
  error: string;
  loading: boolean;
}

class CompanyDialog extends React.Component<CompanyDialogProps, CompanyDialogState> {
  private inputRef = React.createRef<any>();

  constructor(props) {
    super(props);
    this.state = {
      error: '',
      loading: false,
    };
  }

  inputOnChange = () => {
    if (this.state.error !== '') {
      this.setState({
        error: '',
      });
    }
  }

  onSubmit = (event) => {
    event.preventDefault();
    const siteName = this.inputRef.current.value;
    analytics.track('Site check start', { siteName });
    this.setState({
      loading: true,
    });
    checkSiteAvailability(siteName).then((availability) => {
      this.props.onSiteCheck(siteName, availability);
    }).catch((err) => {
      this.setState({
        error: err.message,
      });
    }).then(() => {
      this.setState({
        loading: false,
      });
    });
  }

  render() {
    return (
      <DialogBox width={480}>
        <div className='company-login-content'>
          <h3>{this.props.title}</h3>
          <Form style={{ width: '100%' }} onSubmit={this.onSubmit}>
            <label htmlFor='company'>Site Name</label>
            <InputGroup size='sm'>
              <FormControl
                className='company-login-content--input'
                ref={this.inputRef}
                placeholder='yourcompanyname'
                disabled={this.state.loading}
                onChange={this.inputOnChange}
              />
              <InputGroup.Append>
                <InputGroup.Text id='company'>{'.' + DOMAIN_NAME}</InputGroup.Text>
              </InputGroup.Append>
            </InputGroup>
            <div className='company-login-content--error'>
              {this.state.error}
            </div>
            <Button
              className='company-login-content--submit'
              type='submit'
              variant='info'
              disabled={this.state.loading}
            >
              Continue
            </Button>
            <div className='company-login-content--footer-text'>
              {this.props.footerText + ' '}
              <Link to={this.props.footerLink}>{this.props.footerLinkText}</Link>
            </div>
          </Form>
        </div>
      </DialogBox>
    );
  }
}
