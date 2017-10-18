module.exports = {

  module: {
    rules: [
      { // css loader
        test: /\.css$/,
        use: [ 'style-loader', 'css-loader' ]
      },
      { // html loader
        test: /\.(html)$/,
        use: { loader: 'html-loader' }
      }
    ]
  }

};
