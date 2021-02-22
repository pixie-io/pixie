const { resolve, join } = require('path');
const { execSync } = require('child_process');

const webpack = require('webpack');
const { CheckerPlugin } = require('awesome-typescript-loader');
const CaseSensitivePathsPlugin = require('case-sensitive-paths-webpack-plugin');
const HtmlWebpackPlugin = require('html-webpack-plugin');
const YAML = require('yaml');
const fs = require('fs');
const CompressionPlugin = require('compression-webpack-plugin');
const MiniCssExtractPlugin = require('mini-css-extract-plugin');
const FaviconsWebpackPlugin = require('favicons-webpack-plugin');
const MonacoWebpackPlugin = require('monaco-editor-webpack-plugin');
const utils = require('./webpack-utils');

const isDevServer = process.argv.find((v) => v.includes('serve'));
let topLevelDir = '';
if (isDevServer) {
  topLevelDir = execSync('git rev-parse --show-toplevel').toString().trim();
}

const plugins = [
  new CheckerPlugin(),
  new CaseSensitivePathsPlugin(),
  new FaviconsWebpackPlugin('../assets/favicon-base.png'),
  new HtmlWebpackPlugin({
    alwaysWriteToDisk: true,
    chunks: ['config', 'manifest', 'commons', 'vendor', 'main'],
    chunksSortMode: 'manual',
    template: 'index.html',
    filename: 'index.html',
  }),
  new webpack.EnvironmentPlugin({
    STABLE_BUILD_NUMBER: '0',
    STABLE_BUILD_SCM_REVISION: '0000000',
    STABLE_BUILD_SCM_STATUS: 'Modified',
    BUILD_TIMESTAMP: '0',
  }),
  new webpack.ContextReplacementPlugin(
    /highlight.js[/\\]lib[/\\]languages$/, /javascript|bash|python/,
  ),
  // Uncomment to enabled bundle analysis.
  // new (require('webpack-bundle-analyzer').BundleAnalyzerPlugin),
  new MonacoWebpackPlugin({
    languages: ['json', 'python'],
  }),
];

if (isDevServer) {
  plugins.push(new webpack.SourceMapDevToolPlugin({
    filename: 'sourcemaps/[file].map',
    exclude: [/node_modules/, /vendor/, /vendor\.chunk\.js/, /vendor\.js/],
  }));
} else {
  plugins.push(
    new MiniCssExtractPlugin({
      filename: isDevServer ? '[name].css' : '[name].[contenthash].css',
      chunkFilename: isDevServer ? '[id].css' : '[id].[contenthash].css',
    }),
    new CompressionPlugin({
      algorithm: 'gzip',
      threshold: 1024,
      exclude: /config\.js/,
    }),
    new utils.ArchivePlugin({
      output: join(resolve(__dirname, 'dist'), 'bundle.tar.gz'),
    }),
  );
}

const webpackConfig = {
  context: resolve(__dirname, 'src'),
  devtool: false, // We use the SourceMapDevToolPlugin to generate source-maps.
  devServer: {
    contentBase: resolve(__dirname, 'dist'),
    https: true,
    disableHostCheck: true,
    hot: true,
    writeToDisk: true,
    publicPath: '/static',
    historyApiFallback: true,
    proxy: [],
  },
  entry: {
    main: 'app.tsx',
    config: ['flags.js', 'segment.js'],
  },
  module: {
    rules: [
      {
        test: /\.js[x]?$/,
        loader: require.resolve('babel-loader'),
        options: {
          cacheDirectory: true,
          ignore: ['segment.js'],
        },
      },
      {
        test: /\.ts[x]?$/,
        loader: require.resolve('awesome-typescript-loader'),
      },
      {
        test: /\.(jpg|png|gif|svg)$/,
        loader: 'image-webpack-loader',
        // Specify enforce: 'pre' to apply the loader
        // before url-loader/svg-url-loader
        // and not duplicate it in rules with them
        enforce: 'pre',
      },
      {
        test: /\.(svg)$/i,
        loader: require.resolve('svg-url-loader'),
        options: {
          // Images larger than 10 KB won't be inlined
          limit: 10 * 1024,
          name: 'assets/[name].[contenthash].[ext]',
          noquotes: true,
        },
      },
      {
        test: /\.(jpe?g|png|gif)$/i,
        loader: require.resolve('url-loader'),
        options: {
          // Images larger than 10 KB won't be inlined
          limit: 10 * 1024,
          name: 'assets/[name].[contenthash].[ext]',
        },
      },
      {
        test: /\.css$/,
        use: [
          isDevServer ? 'style-loader' : MiniCssExtractPlugin.loader,
          'css-loader',
        ],
      },
      {
        test: /\.(woff(2)?|ttf|eot)(\?v=\d+\.\d+\.\d+)?$/,
        use: [
          {
            loader: 'file-loader',
            options: {
              name: '[name].[ext]',
              outputPath: 'fonts/',
            },
          },
        ],
      },
    ],
  },
  output: {
    filename: '[name].[contenthash].js',
    chunkFilename: '[name].[contenthash].chunk.js',
    publicPath: '/static/',
  },
  plugins,
  resolve: {
    extensions: [
      '.js',
      '.jsx',
      '.ts',
      '.tsx',
      '.web.js',
      '.webpack.js',
      '.png',
    ],
    modules: ['node_modules', resolve('./src'), resolve('./assets')],
  },
  optimization: {
    splitChunks: {
      cacheGroups: {
        commons: {
          chunks: 'initial',
          test(module) {
            return !module.getChunks().find((c) => c.name === 'config');
          },
          minChunks: 2,
          maxInitialRequests: 5, // The default limit is too small to showcase the effect
          minSize: 0, // This is example is too small to create commons chunks
        },
        vendor: {
          test: /[\\/]node_modules[\\/]/,
          chunks: 'initial',
          name: 'vendor',
          priority: 10,
        },
      },
    },
  },
};

module.exports = (env, argv) => {
  // Always emit files in production mode.
  webpackConfig.output.compareBeforeEmit = argv.mode !== 'production';

  if (!isDevServer) {
    return webpackConfig;
  }

  const sslDisabled = env && Object.prototype.hasOwnProperty.call(env, 'disable_ssl') && env.disable_ssl;
  // Add the Gateway to the proxy config.
  let gatewayPath = process.env.PL_GATEWAY_URL;
  if (!gatewayPath) {
    gatewayPath = `http${sslDisabled ? '' : 's'}://${utils.findGatewayProxyPath()}`;
  }

  webpackConfig.devServer.proxy.push({
    context: ['/api', '/pl.api.vizierpb.VizierService/'],
    target: gatewayPath,
    secure: false,
  });

  // Normally, these values are replaced by Nginx. However, since we do not
  // use nginx for the dev server, we need to replace them here.
  let environment = process.env.PL_BUILD_TYPE;
  if (!environment || environment === 'dev') {
    environment = 'base';
  }

  // Get Auth0ClientID.
  const authYamlPath = join(topLevelDir, 'k8s', 'cloud', environment, 'auth0_config.yaml').replace(/\//g, '\\/');
  // Don't try to change this to `fs.readFileSync` to avoid the useless cat: readFileSync can't find this path, cat can.
  const auth0YamlReq = execSync(`cat ${authYamlPath}`);
  const auth0YAML = YAML.parse(auth0YamlReq.toString());

  // Get LDClientID.
  const ldYamlPath = join(topLevelDir, 'k8s', 'cloud', environment, 'ld_config.yaml').replace(/\//g, '\\/');
  // Don't try to change this to `fs.readFileSync` to avoid the useless cat: readFileSync can't find this path, cat can.
  const ldYamlReq = execSync(`cat ${ldYamlPath}`);
  const ldYAML = YAML.parse(ldYamlReq.toString());

  // Get domain name.
  const domainYamlPath = join(topLevelDir, 'k8s', 'cloud', environment, 'domain_config.yaml').replace(/\//g, '\\/');
  // Don't try to change this to `fs.readFileSync` to avoid the useless cat: readFileSync can't find this path, cat can.
  const domainYamlReq = execSync(`cat ${domainYamlPath}`);
  const domainYAML = YAML.parse(domainYamlReq.toString());

  // Get whether to enable analytics.
  const analyticsYamlPath = join(topLevelDir, 'k8s', 'cloud', environment,
    'analytics_config.yaml').replace(/\//g, '\\/');
  const analyticsYamlReq = execSync(`cat ${analyticsYamlPath}`);
  const analyticsYAML = YAML.parse(analyticsYamlReq.toString());

  // Get whether to enable analytics.
  const anouncementYamlPath = join(topLevelDir, 'k8s', 'cloud', environment,
    'announce_config.yaml').replace(/\//g, '\\/');
  const announcementYamlReq = execSync(`cat ${anouncementYamlPath}`);
  const announcementYAML = YAML.parse(announcementYamlReq.toString());

  // Get whether to enable chat contact.
  const contactYamlPath = join(topLevelDir, 'k8s', 'cloud', environment,
    'contact_config.yaml').replace(/\//g, '\\/');
  const contactYamlReq = execSync(`cat ${contactYamlPath}`);
  const contactYAML = YAML.parse(contactYamlReq.toString());
  webpackConfig.plugins.unshift(
    new webpack.DefinePlugin({
      __CONTACT_ENABLED__: JSON.parse(contactYAML.data.CONTACT_ENABLED),
      __ANNOUNCEMENT_ENABLED__: JSON.parse(announcementYAML.data.ANNOUNCEMENT_ENABLED),
      __ANNOUNCE_WIDGET_URL__: JSON.stringify(announcementYAML.data.ANNOUNCE_WIDGET_URL),
      __ANALYTICS_ENABLED__: JSON.parse(analyticsYAML.data.ANALYTICS_ENABLED),
      __SEGMENT_UI_WRITE_KEY__: '""',
      __CONFIG_AUTH0_DOMAIN__: JSON.stringify('pixie-labs.auth0.com'),
      __CONFIG_AUTH0_CLIENT_ID__: JSON.stringify(auth0YAML.data.PL_AUTH0_CLIENT_ID),
      __CONFIG_DOMAIN_NAME__: JSON.stringify(domainYAML.data.PL_DOMAIN_NAME),
      __CONFIG_LD_CLIENT_ID__: JSON.stringify(ldYAML.data.PL_LD_CLIENT_ID),
      __SEGMENT_ANALYTICS_JS_DOMAIN__: `segment.${domainYAML.data.PL_DOMAIN_NAME}`,
    }),
  );

  if (process.env.SELFSIGN_CERT_FILE && process.env.SELFSIGN_CERT_KEY) {
    const cert = fs.readFileSync(process.env.SELFSIGN_CERT_FILE);
    const key = fs.readFileSync(process.env.SELFSIGN_CERT_KEY);
    webpackConfig.devServer.https = { key, cert };
  } else {
    const credsEnv = environment === 'base' ? 'dev' : environment;
    const certsPath = join(topLevelDir,
      'credentials', 'k8s', credsEnv, 'cloud_proxy_tls_certs.yaml').replace(/\//g, '\\/');
    const results = execSync(`sops --decrypt ${certsPath}`);
    const credsYAML = YAML.parse(results.toString());
    webpackConfig.devServer.https = {
      key: credsYAML.stringData['tls.key'],
      cert: credsYAML.stringData['tls.crt'],
    };
  }

  return webpackConfig;
};
