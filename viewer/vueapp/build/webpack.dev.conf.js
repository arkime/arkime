'use strict'

const utils = require('./utils')
const { git } = require('../../../common/git')
const webpack = require('webpack')
const config = require('../config')
const { merge } = require('webpack-merge')
const path = require('path')
const baseWebpackConfig = require('./webpack.base.conf')
const CopyWebpackPlugin = require('copy-webpack-plugin')
const HtmlWebpackPlugin = require('html-webpack-plugin')
const VueLoaderPlugin = require('vue-loader/lib/plugin')
const ESLintPlugin = require('eslint-webpack-plugin')

const devWebpackConfig = merge(baseWebpackConfig, {
  module: {
    rules: utils.styleLoaders({ sourceMap: config.dev.cssSourceMap, usePostCSS: true })
  },
  stats: 'minimal',
  // cheap-module-eval-source-map is faster for development
  devtool: config.dev.devtool,
  mode: 'development',
  plugins: [
    new webpack.DefinePlugin({
      'process.env': require('../config/dev.env'),
      BUILD_VERSION: JSON.stringify(git('describe --tags')),
      BUILD_DATE: JSON.stringify(git('log -1 --format=%aI'))
    }),
    new webpack.ProvidePlugin({
      $: 'jquery',
      jQuery: 'jquery'
    }),
    new webpack.HotModuleReplacementPlugin(),
    new webpack.NoEmitOnErrorsPlugin(),
    // https://github.com/ampedandwired/html-webpack-plugin
    new HtmlWebpackPlugin({
      filename: config.build.index,
      template: 'index.html',
      inject: true
    }),
    // copy custom static assets
    new CopyWebpackPlugin({
      patterns: [
        {
          from: path.resolve(__dirname, '../static'),
          to: config.dev.assetsSubDirectory,
          globOptions: {
            ignore: ['.*']
          },
        }
      ]
    }),
    new VueLoaderPlugin(),
    new ESLintPlugin({
      emitError: true,
      emitWarning: true,
      failOnError: false,
      extensions: ['js', 'vue'],
      context: path.resolve(__dirname, '../'),
      overrideConfigFile: path.resolve(__dirname, '../../../.eslintrc.js')
    })
  ]
})

module.exports = devWebpackConfig;
