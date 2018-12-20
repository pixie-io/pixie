const topLevelConfig = require('../webpack.config.js')();
const TSDocgenPlugin = require('react-docgen-typescript-webpack-plugin');

module.exports = (baseConfig, env, config) => {
  config.module.rules = topLevelConfig.module.rules;
  config.resolve.extensions = topLevelConfig.resolve.extensions;

  config.plugins.push(new TSDocgenPlugin());

  return config;
};
