import {DialogBox} from 'components/dialog-box/dialog-box';
import * as React from 'react';
import {Button, FormControl, InputGroup} from 'react-bootstrap';
import { Link } from 'react-router-dom';

interface CompanyDialogProps {
  title: string;
  footerText: string;
  footerLink: string;
  footerLinkText: string;
}

export const CompanyLogin = () => {
  return (<CompanyDialog
    title='Log in to your company'
    footerText='Don&apos;t have a company site yet?'
    footerLink='/create'
    footerLinkText='Claim your site here'
  />);
};

export const CompanyCreate = () => {
  return (<CompanyDialog
    title='Claim your site'
    footerText='Already have a site?'
    footerLink='/'
    footerLinkText='Click here to log in'
  />);
};

const CompanyDialog = (props: CompanyDialogProps) => {
  return (
    <DialogBox width={480}>
      <div className='company-login-content'>
      <h3>{props.title}</h3>
      <div style={{width: '100%'}}>
        <label htmlFor='company'>Site Name</label>
        <InputGroup size='sm'>
          <FormControl
            placeholder='yourcompanyname'
          />
          <InputGroup.Append>
            <InputGroup.Text id='company'>.pixielabs.ai</InputGroup.Text>
          </InputGroup.Append>
        </InputGroup>
        <Button variant='info' type='submit'>
          Continue
        </Button>
        <div className='company-login-content--footer-text'>
          {props.footerText + ' '}
          <Link to={props.footerLink}>{props.footerLinkText}</Link>
        </div>
      </div>
    </div>
  </DialogBox>
  );
};
