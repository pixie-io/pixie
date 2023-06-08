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

const CaseSensitivePathsPlugin = require('case-sensitive-paths-webpack-plugin');
const { execSync } = require('child_process');
const CompressionPlugin = require('compression-webpack-plugin');
const FaviconsWebpackPlugin = require('favicons-webpack-plugin');
const fs = require('fs');
const HtmlWebpackPlugin = require('html-webpack-plugin');
const MiniCssExtractPlugin = require('mini-css-extract-plugin');
const MonacoWebpackPlugin = require('monaco-editor-webpack-plugin');
const { resolve, join } = require('path');
const webpack = require('webpack');

const utils = require('./webpack-utils');
const { findConfig, getYamls } = require('./webpack-yaml-configs');

const isDevServer = process.argv.find((v) => v.includes('serve'));
let topLevelDir = '';
if (isDevServer) {
  topLevelDir = execSync('git rev-parse --show-toplevel').toString().trim();
}

const plugins = [
  new CaseSensitivePathsPlugin(),
  new FaviconsWebpackPlugin({
    logo: '../assets/favicon-base.png',
    favicons: {
      icons: {
        android: false,
        appleIcon: false,
        appleStartup: false,
        favicons: true,
        windows: false,
        yandex: false
      }
    }
  }),
  new HtmlWebpackPlugin({
    alwaysWriteToDisk: true,
    chunks: ['config', 'manifest', 'commons', 'vendor', 'main'],
    chunksSortMode: 'manual',
    template: 'index.html',
    filename: 'index.html',
  }),
  new webpack.EnvironmentPlugin({
    STABLE_BUILD_TAG: '0.1.0',
    BUILD_TIMESTAMP: '0',
  }),
  new webpack.ContextReplacementPlugin(
    /highlight.js[/\\]lib[/\\]languages$/, /javascript|bash|python/,
  ),
  // Uncomment to enabled bundle analysis.
  // new (require('webpack-bundle-analyzer').BundleAnalyzerPlugin)({
  //   analyzerMode: 'static',
  //   analyzerHost: '0.0.0.0',
  //   generateStatsFile: true,
  //   openAnalyzer: false,
  // }),
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
    static: {
      directory: resolve(__dirname, 'dist'),
    },
    server: {
      type: 'https',
    },
    client: {
      overlay: {
        errors: true,
        warnings: false,
      },
    },
    allowedHosts: 'all',
    hot: true,
    devMiddleware: {
      publicPath: '/static',
      writeToDisk: true,
    },
    historyApiFallback: {
      disableDotRule: true,
    },
    proxy: [],
  },
  watchOptions: {
    ignored: ['**/node_modules'],
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
          target: 'es2020',
        },
      })),
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
          // Yarn PnP still puts this in the string (`.../.yarn/__virtual__/.../depName/.../node_modules/...`)
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

  // Add the Gateway to the proxy config.
  let gatewayPath = process.env.PL_GATEWAY_URL;
  if (!gatewayPath) {
    throw new Error('Must set PL_GATEWAY_URL when running webpack in dev mode.');
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

  const yamls = getYamls(topLevelDir, environment);

  // Setup the auth client.
  const {
    PL_OAUTH_PROVIDER: oauthProvider,
    PL_AUTH_URI: authURI,
    PL_AUTH_CLIENT_ID: authClientID,
    PL_AUTH_EMAIL_PASSWORD_CONN: authEmailPasswordConnection,
    PL_OIDC_HOST: oidcHost,
    PL_OIDC_METADATA_URL: oidcMetadataURL,
    PL_OIDC_CLIENT_ID: oidcClientID,
    PL_OIDC_ADDITIONAL_SCOPES: oidcAdditionalScopes,
    PL_OIDC_SOCIAL_CONFIG_LOGIN: oidcSocialConfigLogin,
    PL_OIDC_SOCIAL_CONFIG_SIGNUP: oidcSocialConfigSignup,
  } = yamls.oauth.data;

  webpackConfig.plugins.unshift(
    new webpack.DefinePlugin({
      __CONTACT_ENABLED__: JSON.parse(yamls.contact.data.CONTACT_ENABLED),
      __ANNOUNCEMENT_ENABLED__: JSON.parse(yamls.announcement.data.ANNOUNCEMENT_ENABLED),
      __ANNOUNCE_WIDGET_URL__: JSON.stringify(yamls.announcement.data.ANNOUNCE_WIDGET_URL),
      __ANALYTICS_ENABLED__: JSON.parse(yamls.analytics.data.ANALYTICS_ENABLED),
      __PASSTHROUGH_PROXY_PORT__: JSON.stringify(yamls.domain.data.PASSTHROUGH_PROXY_PORT || ''),
      __SEGMENT_UI_WRITE_KEY__: '""',
      __CONFIG_OAUTH_PROVIDER__: JSON.stringify(oauthProvider),
      __CONFIG_AUTH_URI__: JSON.stringify(authURI),
      __CONFIG_AUTH_CLIENT_ID__: JSON.stringify(authClientID),
      __CONFIG_AUTH_EMAIL_PASSWORD_CONN__: JSON.stringify(authEmailPasswordConnection),
      __CONFIG_OIDC_HOST__: JSON.stringify(oidcHost),
      __CONFIG_OIDC_METADATA_URL__: JSON.stringify(oidcMetadataURL),
      __CONFIG_OIDC_CLIENT_ID__: JSON.stringify(oidcClientID),
      __CONFIG_OIDC_ADDITIONAL_SCOPES__: JSON.stringify(oidcAdditionalScopes),
      __CONFIG_OIDC_SOCIAL_CONFIG_LOGIN__: JSON.stringify(oidcSocialConfigLogin),
      __CONFIG_OIDC_SOCIAL_CONFIG_SIGNUP__: JSON.stringify(oidcSocialConfigSignup),
      __CONFIG_DOMAIN_NAME__: JSON.stringify(yamls.domain.data.PL_DOMAIN_NAME),
      __CONFIG_LD_CLIENT_ID__: JSON.stringify(yamls.ld.data.PL_LD_CLIENT_ID),
      __CONFIG_SCRIPT_BUNDLE_URLS__: JSON.stringify(yamls.scriptBundle.data.SCRIPT_BUNDLE_URLS),
      __CONFIG_SCRIPT_BUNDLE_DEV__: JSON.parse(yamls.scriptBundle.data.SCRIPT_BUNDLE_DEV),
      __SEGMENT_ANALYTICS_JS_DOMAIN__: `"segment.${yamls.domain.data.PL_DOMAIN_NAME}"`,
    }),
  );

  if (process.env.SELFSIGN_CERT_FILE && process.env.SELFSIGN_CERT_KEY) {
    const cert = fs.readFileSync(process.env.SELFSIGN_CERT_FILE);
    const key = fs.readFileSync(process.env.SELFSIGN_CERT_KEY);
    webpackConfig.devServer.server.options = { key, cert };
  } else {
    const credsEnv = environment === 'base' ? 'dev' : environment;
    const credsYAML = findConfig(topLevelDir, credsEnv, 'cloud_proxy_tls_certs.yaml', null, true);
    if (!credsYAML.stringData) {
      throw new Error('Could not find proxy TLS certificates! Try setting the SELFSIGN_CERT_{FILE,KEY} env vars.');
    }
    webpackConfig.devServer.server.options = {
      key: credsYAML.stringData['tls.key'],
      cert: credsYAML.stringData['tls.crt'],
    };
  }

  return webpackConfig;
};
