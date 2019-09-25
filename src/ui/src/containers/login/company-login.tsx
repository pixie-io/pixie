import Axios from 'axios';
import {DialogBox} from 'components/dialog-box/dialog-box';
import {DOMAIN_NAME} from 'containers/constants';
import * as React from 'react';
import {Button, Form, FormControl, InputGroup} from 'react-bootstrap';
import { HotKeys } from 'react-hotkeys';
import { Link } from 'react-router-dom';
import * as RedirectUtils from 'utils/redirect-utils';

const HOT_KEY_MAP = {
  CLICK_CONTINUE: ['enter'],
};

interface CompanyDialogProps {
  title: string;
  footerText: string;
  footerLink: string;
  footerLinkText: string;
  onClick: (e) => void;
}

function companyLoginOnClick(e) {
  const domainName = this.inputRef.current.value;
  Axios({
    method: 'get',
    url: '/api/site/check',
    params: {
      domain_name: domainName,
    },
  }).then((response) => {
    if (response.data.available) {
      this.setState({
        error: 'The site doesn\'t exist. Please check the name and try again.',
      });
    } else {
      RedirectUtils.redirect('id', '/login', {['domain_name']: domainName });
    }
  }).catch((error) => {
    this.setState({
      error: 'Could not check site availability: ' + error,
    });
  });
}

function companyCreateOnClick(e) {
  const domainName = this.inputRef.current.value;
  Axios({
    method: 'get',
    url: '/api/site/check',
    params: {
      domain_name: domainName,
    },
  }).then((response) => {
    if (!response.data.available) {
      this.setState({
        error: 'Sorry, the site already exists. Try a different name.',
      });
    } else {
      RedirectUtils.redirect('id', '/create-site', {['domain_name']: domainName});
    }
  }).catch((error) => {
    this.setState({
      error: 'Could not check site availability: ' + error,
    });
  });
}

export const CompanyLogin = () => {
  return (<CompanyDialog
    title='Log in to your company'
    footerText='Don&apos;t have a company site yet?'
    footerLink='/create'
    footerLinkText='Claim your site here'
    onClick={companyLoginOnClick}
  />);
};

export const CompanyCreate = () => {
  return (<CompanyDialog
    title='Claim your site'
    footerText='Already have a site?'
    footerLink='/'
    footerLinkText='Click here to log in'
    onClick={companyCreateOnClick}
  />);
};

interface CompanyDialogState {
  error: string;
}

class CompanyDialog extends React.Component<CompanyDialogProps, CompanyDialogState> {
  private inputRef = React.createRef<any>();

  constructor(props) {
    super(props);
    this.state = {
      error: '',
    };
  }

  inputOnChange = () => {
    if (this.state.error !== '') {
      this.setState({
        error: '',
      });
    }
  }

  render() {
    return (
      <DialogBox width={480}>
          <div className='company-login-content'>
          <h3>{this.props.title}</h3>
          <div style={{width: '100%'}}>
            <label htmlFor='company'>Site Name</label>
            <HotKeys
              className='hotkey-container'
              focused={true}
              keyMap={HOT_KEY_MAP}
              handlers={{ CLICK_CONTINUE: () => {
                if (this.state.error === '') {
                  this.props.onClick.bind(this)();
                }
              } }}
            >
              <InputGroup size='sm'>
                <FormControl
                  ref={this.inputRef}
                  placeholder='yourcompanyname'
                  onChange={this.inputOnChange}
                />
                <InputGroup.Append>
                  <InputGroup.Text id='company'>{'.' + DOMAIN_NAME}</InputGroup.Text>
                </InputGroup.Append>
              </InputGroup>
            </HotKeys>
            <div className='company-login-content--error'>
              {this.state.error}
            </div>
            <Button onClick={this.props.onClick.bind(this)} variant='info' disabled={this.state.error !== ''}>
              Continue
            </Button>
            <div className='company-login-content--footer-text'>
              {this.props.footerText + ' '}
              <Link to={this.props.footerLink}>{this.props.footerLinkText}</Link>
            </div>
          </div>
        </div>
    </DialogBox>
    );
  }
}
