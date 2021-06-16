/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

const { resolve, join } = require('path');
const { execSync } = require('child_process');

const webpack = require('webpack');
const CaseSensitivePathsPlugin = require('case-sensitive-paths-webpack-plugin');
const HtmlWebpackPlugin = require('html-webpack-plugin');
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
    exclude: [/vendor/, /vendor\.chunk\.js/, /vendor\.js/],
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
    historyApiFallback: {
      disableDotRule: true,
    },
    proxy: [],
  },
  entry: {
    main: './app.tsx',
    config: ['./flags.js', './segment.js'],
  },
  module: {
    rules: [
      ...['js', 'jsx', 'ts', 'tsx'].map((ext) => ({
        test: new RegExp(`\\.${ext}$`),
        loader: require.resolve('esbuild-loader'),
        options: {
          loader: ext,
          target: 'es6',
        },
      })),
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
    alias: {
      configurable: [
        resolve(__dirname, 'src/configurables/private/'),
        resolve(__dirname, 'src/configurables/base/'),
      ],
      'app/*': [
        resolve(__dirname, 'src/'),
      ],
      'assets/*': [
        resolve(__dirname, 'assets/'),
      ]
    },
    fallback: {
      // client-oauth2 references `querystring` (nodeJS builtin). We use `query-string` to polyfill it in browsers.
      querystring: require.resolve('query-string'),
    },
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
          // Yarn PnP still puts this in the string (`.../.yarn/$$virtual/.../depName/.../node_modules/...)
          // so we can still split the vendor bundle based on that.
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
    context: ['/api', '/px.api.vizierpb.VizierService/', '/oauth'],
    target: gatewayPath,
    secure: false,
  });

  // Normally, these values are replaced by Nginx. However, since we do not
  // use nginx for the dev server, we need to replace them here.
  let environment = process.env.PL_BUILD_TYPE;
  if (!environment || environment === 'dev') {
    environment = 'base';
  }

  let credentialsEnv = process.env.PL_BUILD_TYPE;

  // Users can specify the OAUTH environment. Usually this just means
  // setting to "ory_auth", otherwise will default to `environment`.
  const oauthConfigEnv = process.env.PL_OAUTH_CONFIG_ENV;
  let oauthYAML = utils.readYAMLFile(
    join(topLevelDir, 'credentials', 'k8s', credentialsEnv, 'configs', 'oauth_config.yaml'), true)
  ;
  // Special case for ory_auth where we read from the unecrypted file.
  if (oauthConfigEnv === 'ory_auth') {
    oauthYAML = utils.readYAMLFile(
      join(topLevelDir, 'k8s', 'cloud', 'base', oauthConfigEnv, 'oauth_config.yaml'), false
    );
  }

  // Setup the auth client.
  const oauthProvider = oauthYAML.data.PL_OAUTH_PROVIDER;
  const authURI = oauthYAML.data.PL_AUTH_URI;
  const authClientID = oauthYAML.data.PL_AUTH_CLIENT_ID;

  // Get LDClientID.
  const ldYAML = utils.readYAMLFile(join(topLevelDir, 'credentials', 'k8s',
    credentialsEnv, 'configs', 'ld_config.yaml'), true);

  // Get domain name.
  const domainYAML = utils.readYAMLFile(join(topLevelDir, 'k8s', 'cloud', environment, 'domain_config.yaml'), false);

  // Get whether to enable analytics.
  const analyticsYAML = utils.readYAMLFile(join(topLevelDir, 'k8s', 'cloud', environment,
    'analytics_config.yaml'), false);

  // Get whether to enable analytics.
  const announcementYAML = utils.readYAMLFile(join(topLevelDir, 'credentials', 'k8s',
    credentialsEnv, 'configs', 'announce_config.yaml'), true);

  // Get whether to enable chat contact.
  const contactYAML = utils.readYAMLFile(join(topLevelDir, 'k8s', 'cloud', environment,
    'contact_config.yaml'), false);

  webpackConfig.plugins.unshift(
    new webpack.DefinePlugin({
      __CONTACT_ENABLED__: JSON.parse(contactYAML.data.CONTACT_ENABLED),
      __ANNOUNCEMENT_ENABLED__: JSON.parse(announcementYAML.data.ANNOUNCEMENT_ENABLED),
      __ANNOUNCE_WIDGET_URL__: JSON.stringify(announcementYAML.data.ANNOUNCE_WIDGET_URL),
      __ANALYTICS_ENABLED__: JSON.parse(analyticsYAML.data.ANALYTICS_ENABLED),
      __SEGMENT_UI_WRITE_KEY__: '""',
      __CONFIG_OAUTH_PROVIDER__: JSON.stringify(oauthProvider),
      __CONFIG_AUTH_URI__: JSON.stringify(authURI),
      __CONFIG_AUTH_CLIENT_ID__: JSON.stringify(authClientID),
      __CONFIG_DOMAIN_NAME__: JSON.stringify(domainYAML.data.PL_DOMAIN_NAME),
      __CONFIG_LD_CLIENT_ID__: JSON.stringify(ldYAML.data.PL_LD_CLIENT_ID),
      __SEGMENT_ANALYTICS_JS_DOMAIN__: `"segment.${domainYAML.data.PL_DOMAIN_NAME}"`,
    }),
  );

  if (process.env.SELFSIGN_CERT_FILE && process.env.SELFSIGN_CERT_KEY) {
    const cert = fs.readFileSync(process.env.SELFSIGN_CERT_FILE);
    const key = fs.readFileSync(process.env.SELFSIGN_CERT_KEY);
    webpackConfig.devServer.https = { key, cert };
  } else {
    const credsEnv = environment === 'base' ? 'dev' : environment;
    const credsYAML = utils.readYAMLFile(join(topLevelDir,
      'credentials', 'k8s', credsEnv, 'cloud_proxy_tls_certs.yaml'), true);
    webpackConfig.devServer.https = {
      key: credsYAML.stringData['tls.key'],
      cert: credsYAML.stringData['tls.crt'],
    };
  }

  return webpackConfig;
};
