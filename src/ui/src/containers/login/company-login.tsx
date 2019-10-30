import Axios from 'axios';
import { DialogBox } from 'components/dialog-box/dialog-box';
import { DOMAIN_NAME } from 'containers/constants';
import * as React from 'react';
import { Button, Form, FormControl, InputGroup } from 'react-bootstrap';
import { HotKeys } from 'react-hotkeys';
import { Link } from 'react-router-dom';
import * as RedirectUtils from 'utils/redirect-utils';

function checkSiteAvailability(siteName: string): Promise<boolean> {
  return Axios({
    method: 'get',
    url: '/api/site/check',
    params: {
      site_name: siteName,
    },
  }).then((response) => {
    return response.data.available;
  }).catch((err) => {
    throw new Error('Failed to check site name availability. Please try again later.');
  });
}

const HOT_KEY_MAP = {
  CLICK_CONTINUE: ['enter'],
};

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
      RedirectUtils.redirect('id', '/login', {['site_name']: siteName });
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
      RedirectUtils.redirect('id', '/create-site', {['site_name']: siteName});
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

  onSubmit = () => {
    this.setState({
      loading: true,
    });
    const siteName = this.inputRef.current.value;
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
          <div style={{width: '100%'}}>
            <label htmlFor='company'>Site Name</label>
            <HotKeys
              className='hotkey-container'
              focused={true}
              keyMap={HOT_KEY_MAP}
              handlers={{ CLICK_CONTINUE: this.onSubmit }}
            >
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
            </HotKeys>
            <div className='company-login-content--error'>
              {this.state.error}
            </div>
            <Button
              className='company-login-content--submit'
              onClick={this.onSubmit}
              variant='info'
              disabled={this.state.loading}
            >
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
