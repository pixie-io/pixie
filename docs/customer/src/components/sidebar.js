import * as React from 'react';
import { StaticQuery, graphql } from 'gatsby';
import styled from 'react-emotion';
import { ExternalLink } from 'react-feather';
import Link from './link';
import './styles.css';
import config from '../../config';

const { forcedNavOrder } = config.sidebar;

const Sidebar = styled('aside')`
  width: 100%;
  /* background-color: rgb(245, 247, 249); */
  /* border-right: 1px solid #ede7f3; */
  height: 100vh;
  overflow: auto;
  position: fixed;
  padding-left: 24px;
  position: -webkit-sticky;
  position: -moz-sticky;
  position: sticky;
  top: 0;
  padding-right: 0;
  background-color: #0b1420;
  //  /* Safari 4-5, Chrome 1-9 */
  //  background: linear-gradient(#0b1420, #132E38);
  //  background: -webkit-gradient(linear, 0% 0%, 0% 100%, from(#0b1420), to(#132E38));
  //  /* Safari 5.1, Chrome 10+ */
  //  background: -webkit-linear-gradient(top, #0b1420, #132E38);
  //  /* Firefox 3.6+ */
  //  background: -moz-linear-gradient(top, #0b1420, #132E38);
  //  /* IE 10 */
  //  background: -ms-linear-gradient(top, #0b1420, #132E38);
  //  /* Opera 11.10+ */
  //  background: -o-linear-gradient(top, #0b1420, #132E38);
`;

// eslint-disable-next-line no-unused-vars
const ListItem = styled(({
  className, active, level, ...props
}) => {
  if (level === 0) {
    return (
      <li className={className}>
        <Link {...props} />
      </li>
    );
  }
  if (level === 1) {
    const customClass = active ? 'active' : '';
    return (
      <li className={`subLevel ${customClass}`}>
        <Link {...props} />
      </li>
    );
  }
  return (
    <li className={className}>
      <Link {...props} />
    </li>
  );
})`
  list-style: none;

  a {
    color: #fff;
    text-decoration: none;
    font-weight: ${({ level }) => (level === 0 ? 700 : 400)};
    padding: 0.45rem 0 0.45rem ${(props) => 2 + (props.level || 0) * 1}rem;
    display: block;
    position: relative;

    &:hover {
      background-color: #132e38;
    }

    ${(props) => props.active
      && `
      color: #fff;
      background-color: #3E8177;
    `} // external link icon
    svg {
      float: right;
      margin-right: 1rem;
    }
  }
`;

const SidebarLayout = ({ location }) => (
  <StaticQuery
    query={graphql`
      query {
        allMdx {
          edges {
            node {
              fields {
                slug
                title
              }
            }
          }
        }
      }
    `}
    render={({ allMdx }) => {
      const navItems = allMdx.edges
        .map(({ node }) => node.fields.slug)
        .filter((slug) => slug !== '/')
        .sort()
        .reduce(
          (acc, cur) => {
            if (forcedNavOrder.find((url) => url === cur)) {
              return { ...acc, [cur]: [cur] };
            }

            const prefix = cur.split('/')[1];

            if (prefix && forcedNavOrder.find((url) => url === `/${prefix}`)) {
              return { ...acc, [`/${prefix}`]: [...acc[`/${prefix}`], cur] };
            }
            return { ...acc, items: [...acc.items, cur] };
          },
          { items: [] },
        );

      /* tslint:disable */
      const nav = forcedNavOrder
        .reduce((acc, cur) => acc.concat(navItems[cur]), [])
        .concat(navItems.items)
        .map((slug) => {
          const { node } = allMdx.edges.find(
            (n2) => n2.node.fields.slug === slug,
          );
          let isActive = false;
          if (
            location
            && (location.pathname === node.fields.slug
              || location.pathname === config.gatsby.pathPrefix + node.fields.slug)
          ) {
            isActive = true;
          }

          return (
            <ListItem
              key={node.fields.slug}
              to={`${node.fields.slug}`}
              level={node.fields.slug.split('/').length - 2}
              active={isActive}
            >
              {node.fields.title}
            </ListItem>
          );
        });
      /* tslint:enable */

      return (
        <Sidebar>
          <ul className="sideBarUL">
            {nav}
            {config.sidebar.links.map((link, key) => {
              if (link.link !== '' && link.text !== '') {
                return (
                  // eslint-disable-next-line
                  <ListItem key={key} to={link.link}>
                    {link.text}
                    <ExternalLink size={14} />
                  </ListItem>
                );
              }
              return null;
            })}
          </ul>
        </Sidebar>
      );
    }}
  />
);

export default SidebarLayout;
