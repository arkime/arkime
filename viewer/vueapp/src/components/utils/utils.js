export default {

  /**
   * Creates a unique random string
   * @returns {string} The unique random string
   */
  createRandomString: function () {
    return Math.random().toString(36).substring(2, 15) + Math.random().toString(36).substring(2, 15);
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
  }

};
