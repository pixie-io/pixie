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
            return (
              <div className='sidebar-nav--item' key={item.link}>
                <Link to={item.link}>
                  <img src={window.location.pathname === item.link ? item.selectedImg : item.unselectedImg }/>
                </Link>
              </div>);
          },
        )}
      </div>
      );
  }
 }
