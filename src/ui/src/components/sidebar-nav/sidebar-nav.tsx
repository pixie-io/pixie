import './sidebar-nav.scss';

import * as React from 'react';
import { Dropdown } from 'react-bootstrap';
import { Link } from 'react-router-dom';

interface StringMap {
  [s: string]: string;
}

export interface SidebarNavItem {
  link?: string;
  selectedImg: string;
  unselectedImg: string;
  menu?: StringMap; // Map from menu item name -> link to redirect to.
  redirect?: boolean; // Whether this should be a hard-redirect rather than a RouteLink.
}

export interface SidebarMenuItemProps {
  menu: StringMap;
  unselectedImg: string;
  selectedImg: string;
}

export interface SidebarNavProps {
  logo: string;
  items: SidebarNavItem[];
  footerItems: SidebarNavItem[];
}

export interface DropdownToggleProps {
  onClick?: (event: any) => void;
}

class DropdownToggle extends React.Component<DropdownToggleProps, {}> {
  constructor(props, context) {
    super(props, context);

    this.handleClick = this.handleClick.bind(this);
  }

  handleClick(e) {
    if (!this.props.onClick) {
      return;
    }

    e.preventDefault();
    this.props.onClick(e);
  }

  render() {
    return (
      <div className='sidebar-nav--link' onClick={this.handleClick}>
        {this.props.children}
      </div>
    );
  }
}

export class SidebarMenuItem extends React.Component<SidebarMenuItemProps, {}> {
  handleClick(e) {
    window.location.href = this.props.menu[e.target.id];
  }

  render() {
    return (<Dropdown>
      <Dropdown.Toggle as={DropdownToggle} id='dropdown-toggle'>
        <img src={this.props.unselectedImg} />
      </Dropdown.Toggle>

      <Dropdown.Menu>
        {
          Object.keys(this.props.menu).map((key) => {
            return (<Dropdown.Item key={key} id={key} onClick={this.handleClick.bind(this)}>{key}</Dropdown.Item>);
          })
        }
      </Dropdown.Menu>
    </Dropdown>);
  }
}

export class SidebarItem extends React.Component<SidebarNavItem, {}> {
  handleClick() {
    window.location.href = this.props.link;
  }

  render() {
    let sidebarItem;
    if (this.props.link) {
      const linkImg = (
        <img src={window.location.pathname === this.props.link ?
          this.props.selectedImg : this.props.unselectedImg} />
      );
      sidebarItem = this.props.redirect ?
        (<div className='sidebar-nav--link' onClick={this.handleClick.bind(this)}>{linkImg}</div>) :
        (<Link to={this.props.link}>{linkImg}</Link>);
    } else {
      sidebarItem = (
        <SidebarMenuItem
          menu={this.props.menu}
          unselectedImg={this.props.unselectedImg}
          selectedImg={this.props.selectedImg}
        />
      );
    }

    return (
      <div className='sidebar-nav--item'>
        {sidebarItem}
      </div>);
  }
}

/**
 * The sidebar-nav is a navigational component located on the side of the page.
 */
export class SidebarNav extends React.Component<SidebarNavProps, {}> {
  render() {
    const logo = this.props.logo;
    const items = this.props.items;
    return (
      <div className='sidebar-nav'>
        <div className='sidebar-nav--logo'>
          <img src={logo} />
        </div>
        {items.map((item, idx) => {
          return <SidebarItem
            key={item.link + '-' + idx}
            link={item.link}
            selectedImg={item.selectedImg}
            unselectedImg={item.unselectedImg}
            menu={item.menu}
            redirect={item.redirect}
          />;
        },
        )}
        <div className='sidebar-nav--spacer' />
        {this.props.footerItems.map((item, idx) => {
          return <SidebarItem
            key={item.link + '-' + idx}
            link={item.link}
            selectedImg={item.selectedImg}
            unselectedImg={item.unselectedImg}
            menu={item.menu}
            redirect={item.redirect}
          />;
        },
        )}
      </div>
    );
  }
}
