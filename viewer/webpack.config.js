const webpack = require('webpack');
let config    = require('./webpack.loaders.js');
const Uglify  = require('uglifyjs-webpack-plugin');

config.context = __dirname + '/app';

config.entry = {
  app: './app.js',
  vendor: ['jquery','angular']
};

config.output = {
  path: __dirname + '/bundle',
  filename: 'app.bundle.js'
};

config.plugins = [
  new Uglify({
    exclude: /(node_modules|public|views)/,
    parallel: true
  }),
  new webpack.optimize.CommonsChunkPlugin({
    name    : 'vendor',
    filename: 'vendor.bundle.js'
  }),
  new webpack.ProvidePlugin({
    $               : 'jquery',
    jQuery          : 'jquery',
    'window.jQuery' : 'jquery'
  })
];

module.exports = config;
