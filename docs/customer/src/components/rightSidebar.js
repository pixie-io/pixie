import * as React from 'react';
import { StaticQuery, graphql } from 'gatsby';
import styled from 'react-emotion';
import './styles.css';
import config from '../../config';

const Sidebar = styled('aside')`
  width: 100%;
  background-color: #fff;
  border-right: 1px solid #dddddd;
  height: 100vh;
  overflow: auto;
  position: fixed;
  padding-left: 24px;
  position: -webkit-sticky;
  position: -moz-sticky;
  position: sticky;
  top: 0;
  @media only screen and (max-width: 50rem) {
    width: 100%;
    position: relative;
  }
`;

const ListItem = styled(({
  className, active, level, ...props
}) => (
  <li className={className}>
    <a href={props.to} {...props} />
  </li>
))`
  list-style: none;

  a {
    color: #5c6975;
    text-decoration: none;
    font-weight: ${({ level }) => (level === 0 ? 700 : 400)};
    padding: 0.45rem 0 0.45rem ${(props) => 2 + (props.level || 0) * 1}rem;
    display: block;
    position: relative;

    &:hover {
      color: #71efce !important;
    }

    ${(props) => props.active
      && `
      color: #71EFCE;
      border-color: rgb(230,236,241) !important;
      border-style: solid none solid solid;
      border-width: 1px 0px 1px 1px;
      background-color: #fff;
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
              }
              tableOfContents
            }
          }
        }
      }
    `}
    render={({ allMdx }) => {
      let finalNavItems;
      if (allMdx.edges !== undefined && allMdx.edges.length > 0) {
        allMdx.edges.forEach((item) => {
          let innerItems;
          if (item !== undefined) {
            if (
              item.node.fields.slug === location.pathname
              || config.gatsby.pathPrefix + item.node.fields.slug
                === location.pathname
            ) {
              if (item.node.tableOfContents.items) {
                innerItems = item.node.tableOfContents.items.map(
                  (innerItem, index) => (
                    // eslint-disable-next-line
                    <ListItem key={index} to={`#${innerItem.title}`} level={1}>
                      {innerItem.title}
                    </ListItem>
                  ),
                );
              }
            }
          }
          if (innerItems) {
            finalNavItems = innerItems;
          }
        });
      }

      if (finalNavItems && finalNavItems.length) {
        return (
          <Sidebar>
            <ul className="rightSideBarUL">
              <div className="rightSideTitle">CONTENTS</div>
              {finalNavItems}
            </ul>
          </Sidebar>
        );
      }
      return (
        <Sidebar>
          <ul />
        </Sidebar>
      );
    }}
  />
);

export default SidebarLayout;
