const config = {
    gatsby: {
        pathPrefix: '/docs',
        siteUrl: 'https://docs.pixielabs.ai',
        gaTrackingId: null,
    },
    header: {
                  logo: '',
            title: '',
                 githubUrl: '',
            helpUrl: '',
        tweetText: '',
        links: [
            { text: '', link: ''},
        ],
    },
    sidebar: {
        forcedNavOrder: [
             '/getting-started',
             '/getting-started/system-overview',
             '/getting-started/compatibility-requirements',
             '/getting-started/support',
             '/admin',
             '/installation',
             '/user-guides',
             '/user-guides/guided-install',
             '/user-guides/query-language-overview',
             '/user-guides/table-schemas',
             '/user-guides/common-queries',
             '/misc',
             '/misc/faq',
             '/misc/product-updates',
             '/misc/privacy-policy',
             '/misc/terms-conditions',
        ],
        links: [
            { text: '', link: ''},
        ],
    },
    siteMetadata: {
               title: 'Pixie Labs Customer Docs',
        description: 'Customer docs for the Pixie platform ',
        ogImage: null,
        docsLocation: 'https://github.com/pixie-labs/pixielabs/docs/customer',
              favicon: 'src/components/images/favicon.svg',
    },
};

module.exports = config;
