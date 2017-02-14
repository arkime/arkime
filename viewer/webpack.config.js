var webpack = require('webpack');
var config  = require('./webpack.loaders.js');

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
  new webpack.optimize.CommonsChunkPlugin('vendor', 'vendor.bundle.js'),
  new webpack.ProvidePlugin({
    $               : 'jquery',
    jQuery          : 'jquery',
    'window.jQuery' : 'jquery'
  })
];

module.exports = config;
