module.exports = {
  /**
   * Sends an error from the server by:
   * 1. setting the http content-type header to json
   * 2. setting the response status code (403 default)
   * 3. sending a false success with message text (default "Server Error!")
   * @param {Object} res - The Express.js response object
   * @param {Number} [resStatus=403] - The response status code (optional)
   * @param {String} [text="Server Error!"] - The response text (optional)
   * @returns {Object} res - The Express.js response object
   */
  serverError: (res, resStatus, text) => {
    res.status(resStatus || 403);
    res.setHeader('Content-Type', 'application/json');
    return res.send(
      { success: false, text: text || 'Server Error!' }
    );
  }
};
