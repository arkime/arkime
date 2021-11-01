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
  sendChunks (subscriber, chunks) {
    for (const chunk of chunks) {
      try {
        const json = JSON.parse(chunk);
        subscriber.next(json);
      } catch (err) {
        subscriber.error(`ERROR: ${err}`);
        break;
      }
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

        return new ReadableStream({
          start () {
            let chunks = [];
            let remaining = '';

            function read () { // handle each data chunk
              reader.read().then(({ done, value }) => {
                if (done) { // stream is done
                  if (chunks.length) {
                    sendChunks(subscriber, chunks);
                  }
                  return subscriber.complete();
                }

                remaining += decoder(value);

                let pos = 0;
                while ((pos = remaining.indexOf('\n')) > -1) {
                  chunks.push(remaining.slice(0, pos)); // process chunk
                  // keep the rest because it may not be complete
                  remaining = remaining.slice(pos + 1, remaining.length);
                }

                if (chunks.length) {
                  sendChunks(subscriber, chunks);
                  chunks = [];
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
