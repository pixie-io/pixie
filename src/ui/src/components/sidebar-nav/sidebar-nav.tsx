import * as React from 'react';
import './sidebar-nav.scss';

import { Link } from 'react-router-dom';

export interface SidebarNavItem {
  link: string;
  selectedImg: string;
  unselectedImg: string;
}

export interface SidebarNavProps {
    logo: string;
    items: SidebarNavItem[];
    footerItems: SidebarNavItem[];
}

export class SidebarItem extends React.Component<SidebarNavItem, {}> {
  render() {
    return (
      <div className='sidebar-nav--item'>
        <Link to={this.props.link}>
          <img src={window.location.pathname === this.props.link ? this.props.selectedImg : this.props.unselectedImg }/>
        </Link>
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
            />;
          },
        )}
      </div>
      );
  }
 }
