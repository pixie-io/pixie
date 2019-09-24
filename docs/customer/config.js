const config = {
    gatsby: {
        pathPrefix: '/',
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
             '/user-guides/query-language-overview',
             '/user-guides/common-queries',
             '/user-guides/guided-install',
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
