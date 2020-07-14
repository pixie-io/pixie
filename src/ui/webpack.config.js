const { resolve, join } = require('path');
const { execSync } = require('child_process');

const webpack = require('webpack');
const { CheckerPlugin } = require('awesome-typescript-loader');
const ArchivePlugin = require('webpack-archive-plugin');
const CaseSensitivePathsPlugin = require('case-sensitive-paths-webpack-plugin');
const HtmlWebpackHarddiskPlugin = require('html-webpack-harddisk-plugin');
const HtmlWebpackPlugin = require('html-webpack-plugin');
const ReplacePlugin = require('webpack-plugin-replace');
const YAML = require('yaml');
const fs = require('fs');
const CompressionPlugin = require('compression-webpack-plugin');
const MiniCssExtractPlugin = require('mini-css-extract-plugin');
const FaviconsWebpackPlugin = require('favicons-webpack-plugin');
const MonacoWebpackPlugin = require('monaco-editor-webpack-plugin');
const utils = require('./webpack-utils');

const isDevServer = process.argv.find((v) => v.includes('webpack-dev-server'));
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
  new HtmlWebpackHarddiskPlugin(),
  new webpack.EnvironmentPlugin([
    'BUILD_ENV',
    'BUILD_NUMBER',
    'BUILD_SCM_REVISION',
    'BUILD_SCM_STATUS',
    'BUILD_TIMESTAMP',
  ]),
  new webpack.ContextReplacementPlugin(
    /highlight.js[/\\]lib[/\\]languages$/, /javascript|bash|python/,
  ),
  new MiniCssExtractPlugin({
    filename: '[name].[hash].css',
    chunkFilename: '[id].[hash].css',
  }),
  // Uncomment to enabled bundle analysis.
  // new (require('webpack-bundle-analyzer').BundleAnalyzerPlugin),
  new MonacoWebpackPlugin({
    languages: ['json', 'python'],
  }),
];

if (isDevServer) {
  // enable HMR globally
  plugins.push(new webpack.HotModuleReplacementPlugin());
  // prints more readable module names in the browser console on HMR updates
  plugins.push(new webpack.NamedModulesPlugin());

  plugins.push(new webpack.SourceMapDevToolPlugin({
    filename: 'sourcemaps/[file].map',
    exclude: [/node_modules/, /vendor/, /vendor\.chunk\.js/, /vendor\.js/],
  }));
} else {
  // Archive plugin has problems with dev server.
  plugins.push(
    new CompressionPlugin({
      algorithm: 'gzip',
      threshold: 1024,
      exclude: /config\.js/,
    }),
    new ArchivePlugin({
      output: join(resolve(__dirname, 'dist'), 'bundle'),
      format: ['tar'],
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
  mode: isDevServer ? 'development' : 'production',
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
        test: /\.(sc|c)ss$/,
        use: [
          {
            loader: MiniCssExtractPlugin.loader,
            options: {
              hmr: isDevServer,
            },
          },
          {
            loader: 'css-loader',
          },
          {
            loader: 'sass-loader',
            options: {
              includePaths: ['node_modules'],
            },
          },
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
    filename: '[name].[hash].js',
    chunkFilename: '[name].[hash].chunk.js',
    path: resolve(__dirname, 'dist'),
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
          test(module, chunks) {
            return !chunks.find((c) => c.name === 'config');
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

module.exports = (env) => {
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
  const auth0YamlReq = execSync(`cat ${authYamlPath}`);
  const auth0YAML = YAML.parse(auth0YamlReq.toString());

  // Get domain name.
  const domainYamlPath = join(topLevelDir, 'k8s', 'cloud', environment, 'domain_config.yaml').replace(/\//g, '\\/');
  const domainYamlReq = execSync(`cat ${domainYamlPath}`);
  const domainYAML = YAML.parse(domainYamlReq.toString());

  webpackConfig.plugins.push(
    new ReplacePlugin({
      include: [
        'flags.js',
        'segment.js',
      ],
      values: {
        __CONFIG_AUTH0_DOMAIN__: 'pixie-labs.auth0.com',
        __CONFIG_AUTH0_CLIENT_ID__: auth0YAML.data.PL_AUTH0_CLIENT_ID,
        __CONFIG_DOMAIN_NAME__: domainYAML.data.PL_DOMAIN_NAME,
        __SEGMENT_ANALYTICS_JS_DOMAIN__: `segment.${domainYAML.data.PL_DOMAIN_NAME}`,
      },
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
