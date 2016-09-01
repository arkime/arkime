module.exports = {

  module: {
    loaders: [
      { // js babel loader (traspiler)
        loader: 'babel',
        exclude: /(node_modules|public|views)/,
        test: /\.js$/,
        query: {
          presets: ['es2015', 'stage-0']
        }
      },
      { // css loader
        test: /\.css$/,
        loaders: ['style', 'css']
      },
      { // scss loader
        test: /\.scss$/,
        exclude: /(node_modules)/,
        loaders: ['style', 'css', 'sass']
      },
      { // html loader
        test: '\.html$/',
        loader: 'html'
      },
      { // font loader for bootstrap
        // http://stackoverflow.com/questions/34840653/cannot-make-bootstrap-work-with-webpack
        test: /\.(ttf|eot|svg|woff(2)?)(\?[a-z0-9]+)?$/,
        loader: 'file-loader',
      },
      { // png loader
        test: /\.png$/,
        loader: 'url-loader?mimetype=image/png'
      },
      { // load jquery before angular
        // https://github.com/webpack/webpack/issues/582
        test: /jquery(\.min)?\.js$/,
        loader: 'expose?jQuery'
      }
    ]
  }

};
