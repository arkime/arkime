import uuid from 'uuidv4';

export default {

  /**
   * Creates a unique random string
   * @returns {string} The unique random string
   */
  createRandomString: function () {
    return uuid();
  },

  /** @returns the default sessions table state if none is defined by the user */
  getDefaultTableState: function () {
    return {
      order: [['firstPacket', 'desc']],
      visibleHeaders: [
        'firstPacket',
        'lastPacket',
        'src',
        'srcPort',
        'dst',
        'dstPort',
        'totPackets',
        'dbby',
        'node',
        'info'
      ]
    };
  },

  /**
   * Finds all of the factors of a given number
   * @param {int} An integer to find factors for
   * @returns {array} An array of factors
   */
  findFactors: function (num) {
    let numArr = [...Array(num).keys()];
    return numArr.filter((i) => { return num % i === 0; });
  }

};
