const config = {
  gatsby : {
    pathPrefix : '/docs',
    siteUrl : 'https://docs.pixielabs.ai',
    gaTrackingId : null,
  },
  header : {
    logo : '',
    title : '',
    githubUrl : '',
    helpUrl : '',
    tweetText : '',
    links : [
      {text : '', link : ''},
    ],
  },
  sidebar : {
    forcedNavOrder : [
      '/getting-started',
      '/getting-started/system-overview',
      '/getting-started/compatibility-requirements',
      '/getting-started/support',
      '/admin',
      '/installation',
      '/installation/deployment-steps',
      '/installation/guided-install',
      '/installation/upgrade',
      '/installation/uninstall',
      '/user-guides',
      '/user-guides/how-to-videos',
      '/user-guides/common-queries',
      '/user-guides/product-faqs',
      '/query-lang',
      '/query-lang/operators',
      '/query-lang/table-schemas',
      '/query-lang/scalar-functions',
      '/query-lang/aggregate-functions',
      '/query-lang/time-in-queries',
      '/misc',
      '/misc/product-updates',
      '/misc/privacy-policy',
      '/misc/terms-conditions',
    ],
    links : [
      {text : '', link : ''},
    ],
  },
  siteMetadata : {
    title : 'Pixie Labs Customer Docs',
    description : 'Customer docs for the Pixie platform ',
    ogImage : null,
    docsLocation : 'https://github.com/pixie-labs/pixielabs/docs/customer',
    favicon : 'src/components/images/favicon.svg',
  },
};

module.exports = config;
