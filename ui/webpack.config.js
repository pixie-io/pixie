const { resolve, join } = require('path');

const webpack = require('webpack');
const { CheckerPlugin } = require('awesome-typescript-loader');
const ArchivePlugin = require('webpack-archive-plugin');
const CaseSensitivePathsPlugin = require('case-sensitive-paths-webpack-plugin');
const HtmlWebpackHarddiskPlugin = require('html-webpack-harddisk-plugin');
const HtmlWebpackPlugin = require('html-webpack-plugin');

const isDevServer = process.argv.find(v => v.includes('webpack-dev-server'));

let plugins = [
  new CheckerPlugin(),
  new CaseSensitivePathsPlugin(),
  new webpack.HotModuleReplacementPlugin(), // enable HMR globally
  new webpack.NamedModulesPlugin(), // prints more readable module names in the browser console on HMR updates
  new HtmlWebpackPlugin({
    alwaysWriteToDisk: true,
    chunks: ['main', 'manifest', 'commons', 'vendor'],
    template: 'index.html',
  }),
  new HtmlWebpackHarddiskPlugin(),
];

// Archive plugin has problems with dev server.
if (!isDevServer) {
  plugins.push(
    new ArchivePlugin({
      output: join(resolve(__dirname, 'dist'), 'bundle'),
      format: ['tar'],
    }));
}

module.exports = {
  context: resolve(__dirname, 'src'),
  devtool: 'source-map',
  devServer: {
    contentBase: resolve(__dirname, 'dist'),
    disableHostCheck: true,
    hot: true,
    publicPath: '/',
    historyApiFallback: true,
    proxy: {
      '/api': {
        target: 'https://api-service:30010',
        secure: false,
      },
    },
  },
  entry: [require.resolve('react-dev-utils/webpackHotDevClient'), 'index.tsx'],
  mode: isDevServer ? 'development' : 'production',
  module: {
    rules: [
      {
        test: /\.js[x]?$/,
        loader: require.resolve('babel-loader'),
        options: {
          cacheDirectory: true,
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
          limit: 10000,
          name: 'static/media/[name].[hash:8].[ext]',
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
      '.json',
      '.jsx',
      '.ts',
      '.tsx',
      '.web.js',
      '.webpack.js',
    ],
    modules: ['node_modules', resolve('./src')],
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
