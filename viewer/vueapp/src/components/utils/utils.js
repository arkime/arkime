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
  }

};
