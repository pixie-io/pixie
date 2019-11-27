// @ts-ignore : TS does not like image files.
import * as userImage from 'images/icons/user.svg';
// @ts-ignore : TS does not like image files.
import * as logoImage from 'images/pixieLogo-light.svg';
import * as React from 'react';
import {Dropdown, Nav, Navbar, NavDropdown} from 'react-bootstrap';
import {Link, NavLink} from 'react-router-dom';

export function VizierTopNav() {
  return (
    <Navbar style={{ height: '48px' }} variant='dark' bg='dark'>
      <Navbar.Brand as={Link} to='/'>
        <img src={logoImage} />
      </Navbar.Brand>
      <Nav style={{ marginRight: 'auto' }}>
        <Nav.Link as={NavLink} to='/vizier/query' activeClassName='active-nav-link'>Console</Nav.Link>
      </Nav>
      <NavDropdown alignRight title={<img src={userImage} />} id='profile-icon-dropdown'>
        <NavDropdown.Item as={NavLink} to='/vizier/agents'>Admin</NavDropdown.Item>
        <NavDropdown.Item href='/docs/getting-started' target='_blank'>Docs</NavDropdown.Item>
        <Dropdown.Divider />
        <NavDropdown.Item as={NavLink} to='/logout'>Logout</NavDropdown.Item>
      </NavDropdown>
    </Navbar>
  );
}
