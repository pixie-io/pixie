import './top-nav.scss';

// @ts-ignore : TS does not like image files.
import * as userImage from 'images/icons/user.svg';
// @ts-ignore : TS does not like image files.
import * as logoImage from 'images/new-logo.svg';
import * as React from 'react';
import {Dropdown, Nav, Navbar, NavDropdown} from 'react-bootstrap';
import {Link, NavLink} from 'react-router-dom';
import {getRedirectPath} from 'utils/redirect-utils';

export function VizierTopNav() {
  return (
    <Navbar style={{ height: '48px' }} variant='dark' bg='primary'>
      <Navbar.Brand as={Link} to='/'>
        <img src={logoImage} style={{ width: '60px' }}/>
      </Navbar.Brand>
      <Nav style={{ marginRight: 'auto' }}>
        <Nav.Link as={NavLink} to='/console' activeClassName='pixie-nav-link-active'>Console</Nav.Link>
      </Nav>
      <NavDropdown alignRight title={<img src={userImage} />} id='profile-icon-dropdown'>
        <NavDropdown.Item as={NavLink} to='/agents'>Admin</NavDropdown.Item>
        <NavDropdown.Item href='/docs/getting-started' target='_blank'>Docs</NavDropdown.Item>
        <Dropdown.Divider />
        <NavDropdown.Item href={'/logout'}>Logout</NavDropdown.Item>
      </NavDropdown>
    </Navbar>
  );
}
