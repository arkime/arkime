import Observable from './Observable';

export default {
  /**
   * Decodes an array of 8-bit unsigned integers into text
   * TODO document */
  decoder (arr) {
    const dec = new TextDecoder('utf-8');
    return dec.decode(arr);
  },

  /**
   * Sends a list of chunks to the subscriber
   * TODO document */
  sendChunks (subscriber, values) {
    for (const val of values) {
      subscriber.next(JSON.parse(val));
    }
  },

  /**
   * Gets stuff
   * TODO document
   */
  search (searchTerm) {
    return new Observable((subscriber) => {
      searchTerm = searchTerm.trim();

      if (!searchTerm) { // nothing to do
        return subscriber.complete();
      }

      fetch(`/api/integration/search/${searchTerm}`).then((response) => {
        if (!response.ok) { // test for bad response code (only on first chunk)
          subscriber.error(response.statusText);
          return;
        }
        return response.body;
      }).then((rStream) => {
        const reader = rStream.getReader();
        const decoder = this.decoder;
        const sendChunks = this.sendChunks;
        let values = [];

        return new ReadableStream({
          start () {
            function read () { // handle each data chunk
              reader.read().then(({ done, value }) => {
                if (done) { // stream is done
                  if (values.length) {
                    sendChunks(subscriber, values);
                  }
                  return subscriber.complete();
                }

                value = decoder(value);

                const regex = /\n/g;
                let startIndex = 0;
                let hasNewline = false;
                while (regex.exec(value) !== null) {
                  values.push(value.slice(startIndex, regex.lastIndex));
                  startIndex = regex.lastIndex;
                  hasNewline = true;
                }

                if (hasNewline || values.length) {
                  sendChunks(subscriber, values);
                  values = [];
                } else {
                  values.push(value);
                }

                read(); // keep reading until done
              });
            }

            read();
          }
        });
      }).catch((err) => { // this catches an issue with in the ^ .then
        subscriber.error(`ERROR: ${err}`);
      });
    });
  }
};
