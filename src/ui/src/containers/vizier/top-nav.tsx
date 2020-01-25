import './top-nav.scss';

// @ts-ignore : TS does not like image files.
import * as userImage from 'images/icons/user.svg';
// @ts-ignore : TS does not like image files.
import * as logoImage from 'images/pixieLogo-light.svg';
import * as React from 'react';
import {Dropdown, Nav, Navbar, NavDropdown} from 'react-bootstrap';
import {Link, NavLink} from 'react-router-dom';
import {getRedirectPath} from 'utils/redirect-utils';

const LOGOUT_URL = getRedirectPath('id', '/logout', {});

export function VizierTopNav() {
  return (
    <Navbar style={{ height: '48px' }} variant='dark' bg='primary'>
      <Navbar.Brand as={Link} to='/'>
        <img src={logoImage} />
      </Navbar.Brand>
      <Nav style={{ marginRight: 'auto' }}>
        <Nav.Link as={NavLink} to='/console' activeClassName='pixie-nav-link-active'>Console</Nav.Link>
      </Nav>
      <NavDropdown alignRight title={<img src={userImage} />} id='profile-icon-dropdown'>
        <NavDropdown.Item as={NavLink} to='/agents'>Admin</NavDropdown.Item>
        <NavDropdown.Item href='/docs/getting-started' target='_blank'>Docs</NavDropdown.Item>
        <Dropdown.Divider />
        <NavDropdown.Item href={LOGOUT_URL}>Logout</NavDropdown.Item>
      </NavDropdown>
    </Navbar>
  );
}
