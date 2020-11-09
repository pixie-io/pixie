const topLevelConfig = require('../webpack.config.js')();
const path = require('path');
const MiniCssExtractPlugin = require("mini-css-extract-plugin");

module.exports = {
  stories: ["../stories/*.tsx"],
  addons: [
    {
      name: '@storybook/addon-essentials',
      options: {
        backgrounds: false,
        controls: false,
      },
    },
  ],
  loaders: [ '@storybook/source-loader' ],
  webpackFinal: (config) => {
    config.module.rules = topLevelConfig.module.rules;
    config.resolve.extensions = topLevelConfig.resolve.extensions;

    config.resolve.modules = ['node_modules', path.resolve(__dirname, '../src'), path.resolve(__dirname, '../assets')];
    config.plugins.push(new MiniCssExtractPlugin({
        filename: "[name].[hash].css",
    }));
    return config;
  },
};
