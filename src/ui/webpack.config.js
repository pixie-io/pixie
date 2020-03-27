const {resolve, join} = require('path');
const {execSync} = require('child_process');

const webpack = require('webpack');
const {CheckerPlugin} = require('awesome-typescript-loader');
const ArchivePlugin = require('webpack-archive-plugin');
const CaseSensitivePathsPlugin = require('case-sensitive-paths-webpack-plugin');
const HtmlWebpackHarddiskPlugin = require('html-webpack-harddisk-plugin');
const HtmlWebpackPlugin = require('html-webpack-plugin');
const utils = require('./webpack-utils');
const ReplacePlugin = require('webpack-plugin-replace');
const YAML = require('yaml');
const fs = require('fs');

const isDevServer = process.argv.find(v => v.includes('webpack-dev-server'));
let topLevelDir = '';
if (isDevServer) {
  topLevelDir = execSync('git rev-parse --show-toplevel').toString().trim();
}

let plugins = [
  new CheckerPlugin(),
  new CaseSensitivePathsPlugin(),
  new HtmlWebpackPlugin({
    alwaysWriteToDisk: true,
    chunks: ['main', 'manifest', 'commons', 'vendor'],
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

  entryFiles = [require.resolve('react-dev-utils/webpackHotDevClient'), 'index.tsx'];
} else {
  // Archive plugin has problems with dev server.
  plugins.push(
    new ArchivePlugin({
      output: join(resolve(__dirname, 'dist'), 'bundle'),
      format: ['tar'],
    }));
}

var webpackConfig = {
  context: resolve(__dirname, 'src'),
  devtool: false, // We use the SourceMapDevToolPlugin to generate source-maps.
  devServer: {
    contentBase: resolve(__dirname, 'dist'),
    https: true,
    disableHostCheck: true,
    hot: true,
    publicPath: '/',
    historyApiFallback: true,
    proxy: [],
  },
  entry: {
    main: 'main.tsx',
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
        test: /\.(jpe?g|png|gif|svg)$/i,
        loader: require.resolve('url-loader'),
        options: {
          limit: 100,
          name: 'assets/[name].[hash:8].[ext]',
        },
      },
      {
        test: /\.scss$/,
        use: [
          {
            loader: 'style-loader',
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
        test: /\.css$/,
        use: ['style-loader', 'css-loader'],
      },
      {
        test: /\.toml$/i,
        use: [
          {
            loader: 'raw-loader',
            options: {
              esModule: false,
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
    filename: '[name].js',
    chunkFilename: '[name].chunk.js',
    path: resolve(__dirname, 'dist'),
    publicPath: '/',
  },
  plugins: plugins,
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
          minChunks: 2,
          maxInitialRequests: 5, // The default limit is too small to showcase the effect
          minSize: 0, // This is example is too small to create commons chunks
        },
        vendor: {
          test: /node_modules/,
          chunks: 'initial',
          name: 'vendor',
          priority: 10,
          enforce: true,
        },
      },
    },
  },
};

module.exports = (env) => {
  if (!isDevServer) {
    return webpackConfig;
  }

  const sslDisabled = env && env.hasOwnProperty('disable_ssl') && env.disable_ssl;
  // Add the Gateway to the proxy config.
  let gatewayPath = process.env.PL_GATEWAY_URL;
  if (!gatewayPath) {
    gatewayPath =
      'http' + (sslDisabled ? '' : 's') + '://' + utils.findGatewayProxyPath();
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
  authYamlPath = join(topLevelDir, 'k8s', 'cloud', environment, 'auth0_config.yaml').replace(/\//g, '\\/');
  auth0YamlReq = execSync('cat ' + authYamlPath);
  auth0YAML = YAML.parse(auth0YamlReq.toString());

  // Get domain name.
  domainYamlPath = join(topLevelDir, 'k8s', 'cloud', environment, 'domain_config.yaml').replace(/\//g, '\\/');
  domainYamlReq = execSync('cat ' + domainYamlPath);
  domainYAML = YAML.parse(domainYamlReq.toString());

  webpackConfig.plugins.push(
    new ReplacePlugin({
      include: [
        'containers/constants.tsx',
        'segment.js',
      ],
      values: {
        __CONFIG_AUTH0_DOMAIN__: 'pixie-labs.auth0.com',
        __CONFIG_AUTH0_CLIENT_ID__: auth0YAML.data.PL_AUTH0_CLIENT_ID,
        __CONFIG_DOMAIN_NAME__: domainYAML.data.PL_DOMAIN_NAME,
        __SEGMENT_ANALYTICS_JS_DOMAIN__: `segment.${domainYAML.data.PL_DOMAIN_NAME}`,
      },
    }));

  if (process.env.SELFSIGN_CERT_FILE && process.env.SELFSIGN_CERT_KEY) {
    const cert = fs.readFileSync(process.env.SELFSIGN_CERT_FILE);
    const key = fs.readFileSync(process.env.SELFSIGN_CERT_KEY);
    webpackConfig.devServer.https = {key, cert};
  } else {
    let credsEnv = environment === 'base' ? 'dev' : environment;
    let certsPath =
      join(topLevelDir, 'credentials', 'k8s', credsEnv, 'cloud_proxy_tls_certs.yaml').replace(/\//g, '\\/');
    let results = execSync('sops --decrypt ' + certsPath);
    let credsYAML = YAML.parse(results.toString());
    webpackConfig.devServer.https = {
      key: credsYAML.stringData['tls.key'],
      cert: credsYAML.stringData['tls.crt'],
    };
  }

  return webpackConfig;
};
