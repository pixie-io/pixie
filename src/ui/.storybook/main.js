const topLevelConfig = require('../webpack.config.js')();
const TSDocgenPlugin = require('react-docgen-typescript-webpack-plugin');
const path = require('path');

module.exports = {
  webpackFinal: (config) => {
    config.module.rules = topLevelConfig.module.rules;
    config.resolve.extensions = topLevelConfig.resolve.extensions;

    config.resolve.modules = ['node_modules', path.resolve(__dirname, '../src'), path.resolve(__dirname, '../assets')];
    config.plugins.push(new TSDocgenPlugin());

    return config;
  },
};
