import * as React from 'react';
import './sidebar-nav.scss';

import {Dropdown} from 'react-bootstrap';
import { Link } from 'react-router-dom';

interface StringMap {
  [s: string]: string;
}

export interface SidebarNavItem {
  link?: string;
  selectedImg: string;
  unselectedImg: string;
  menu?: StringMap; // Map from menu item name -> link to redirect to.
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
  onClick: (event: any) => void;
}

class DropdownToggle extends React.Component<DropdownToggleProps, {}> {
  constructor(props, context) {
    super(props, context);

    this.handleClick = this.handleClick.bind(this);
  }

  handleClick(e) {
    e.preventDefault();

    this.props.onClick(e);
  }

  render() {
    return (
      <a href='' onClick={this.handleClick}>
         {this.props.children}
      </a>
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
        <img src={this.props.unselectedImg}/>
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
  render() {
    return (
      <div className='sidebar-nav--item'>
        {
          this.props.link ?
            <Link to={this.props.link}>
              <img src={window.location.pathname === this.props.link ?
                this.props.selectedImg : this.props.unselectedImg }/>
            </Link> :
            <SidebarMenuItem
              menu={this.props.menu}
              unselectedImg={this.props.unselectedImg}
              selectedImg={this.props.selectedImg}
            />
        }

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
          <img src={logo}/>
        </div>
        {items.map((item) => {
            return <SidebarItem
              key={item.link}
              link={item.link}
              selectedImg={item.selectedImg}
              unselectedImg={item.unselectedImg}
              menu={item.menu}
            />;
          },
        )}
        <div className='sidebar-nav--spacer'/>
        {this.props.footerItems.map((item) => {
            return <SidebarItem
              key={item.link}
              link={item.link}
              selectedImg={item.selectedImg}
              unselectedImg={item.unselectedImg}
              menu={item.menu}
            />;
          },
        )}
      </div>
      );
  }
 }
