import React, { Component } from 'react';
import Helmet from 'react-helmet';
import { graphql } from 'gatsby';
import MDXRenderer from 'gatsby-plugin-mdx/mdx-renderer';
import { injectGlobal } from 'react-emotion';
import { ApolloProvider } from '@apollo/react-hooks';
// eslint-disable-next-line
import { Layout } from '$components';
import NextPrevious from '../components/NextPrevious';
import '../components/styles.css';
import config from '../../config';
import cloudClient from '../apollo/client';

const { forcedNavOrder } = config.sidebar;

// eslint-disable-next-line
injectGlobal`
  * {
    margin: 0;
    padding: 0;
    box-sizing: border-box;
  }

  html, body {
    font-family: -apple-system,
      BlinkMacSystemFont,
      "Segoe UI",
      "Roboto",
      "Roboto Light",
      "Oxygen",
      "Ubuntu",
      "Cantarell",
      "Fira Sans",
      "Droid Sans",
      "Helvetica Neue",
      sans-serif,
      "Apple Color Emoji",
      "Segoe UI Emoji",
      "Segoe UI Symbol";

    font-size: 16px;
  }

  a {
    transition: color 0.15s;
    color: #663399;
  }
`;

export default class MDXRuntimeTest extends Component {
  render() {
    const { data } = this.props;
    const {
      allMdx,
      mdx,
      site: {
        siteMetadata: { title },
      },
    } = data;
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

    const nav = forcedNavOrder
      .reduce((acc, cur) => acc.concat(navItems[cur]), [])
      .concat(navItems.items)
      .map((slug) => {
        if (slug) {
          const { node } = allMdx.edges.find(
            (v) => v.node.fields.slug === slug,
          );

          return { title: node.fields.title, url: node.fields.slug };
        }
        return {};
      });

    // meta tags
    const { metaTitle } = mdx.frontmatter;
    const { metaDescription } = mdx.frontmatter;
    let canonicalUrl = config.gatsby.siteUrl;
    canonicalUrl = config.gatsby.pathPrefix !== '/'
      ? canonicalUrl + config.gatsby.pathPrefix
      : canonicalUrl;
    canonicalUrl += mdx.fields.slug;

    return (
      <Layout {...this.props}>
        <ApolloProvider client={cloudClient}>
          <Helmet>
            {metaTitle ? <title>{metaTitle}</title> : null}
            {metaTitle ? <meta name='title' content={metaTitle} /> : null}
            {metaDescription ? (
              <meta name='description' content={metaDescription} />
            ) : null}
            {metaTitle ? (
              <meta property='og:title' content={metaTitle} />
            ) : null}
            {metaDescription ? (
              <meta property='og:description' content={metaDescription} />
            ) : null}
            {metaTitle ? (
              <meta property='twitter:title' content={metaTitle} />
            ) : null}
            {metaDescription ? (
              <meta property='twitter:description' content={metaDescription} />
            ) : null}
            <link rel='canonical' href={canonicalUrl} />
          </Helmet>
          <div className='titleWrapper'>
            <h1 className='title'>{mdx.fields.title}</h1>
          </div>
          <div className='mainWrapper'>
            <MDXRenderer>{mdx.body}</MDXRenderer>
          </div>
          <div className='addPaddTopBottom'>
            <NextPrevious mdx={mdx} nav={nav} />
          </div>
        </ApolloProvider>
      </Layout>
    );
  }
}

export const pageQuery = graphql`
  query($id: String!) {
    site {
      siteMetadata {
        title
        docsLocation
      }
    }
    mdx(fields: { id: { eq: $id } }) {
      fields {
        id
        title
        slug
      }
      body
      tableOfContents
      parent {
        ... on File {
          relativePath
        }
      }
      frontmatter {
        metaTitle
        metaDescription
      }
    }
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
`;
