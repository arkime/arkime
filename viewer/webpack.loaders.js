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
      { // html loader
        test: '\.html$/',
        loader: 'html'
      },
      { // json loader
        test: '\.json$/',
        loader: 'json'
      },
      { // font loader for bootstrap
        // http://stackoverflow.com/questions/34840653/cannot-make-bootstrap-work-with-webpack
        test: /\.(ttf|eot|svg|woff(2)?)(\?[a-z0-9]+)?$/,
        loader: 'file-loader',
      },
      { // png loader
        test: /\.png$/,
        loader: 'url-loader?mimetype=image/png'
      }
    ]
  }

};
