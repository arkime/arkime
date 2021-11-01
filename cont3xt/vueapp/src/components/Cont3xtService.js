import Observable from './Observable';

export default {
  /**
   * Decodes an array of 8-bit unsigned integers into text
   * TODO document */
  decoder (arr) {
    const dec = new TextDecoder('utf-8');
    return dec.decode(arr);
  },

  /** TODO document */
  sendChunk (subscriber, chunk) {
    // Skip the chunk that is just [
    if (chunk.length < 2) { return; }

    try { // try to parse and send the chunk
      const json = JSON.parse(chunk);
      subscriber.next(json);
    } catch (err) {
      subscriber.error(`ERROR: ${err}`);
      console.log(`Error parsing this chunk:\n${chunk}`);
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

      searchTerm = encodeURIComponent(searchTerm);

      fetch(`api/integration/search/${searchTerm}`).then((response) => {
        if (!response.ok) { // test for bad response code (only on first chunk)
          throw new Error(response.statusText);
        }
        return response.body;
      }).then((rStream) => {
        const reader = rStream.getReader();
        const sendChunk = this.sendChunk;
        const decoder = this.decoder;

        return new ReadableStream({
          start () {
            let remaining = '';

            function read () { // handle each data chunk
              reader.read().then(({ done, value }) => {
                if (done) { // stream is done
                  if (remaining.length) {
                    sendChunk(subscriber, remaining);
                  }
                  return subscriber.complete();
                }

                remaining += decoder(value);

                let pos = 0;
                while ((pos = remaining.indexOf('\n')) > -1) {
                  // - 1 = remove the trailing , or ]
                  sendChunk(subscriber, remaining.slice(0, pos - 1));
                  // keep the rest because it may not be complete
                  remaining = remaining.slice(pos + 1, remaining.length);
                }

                read(); // keep reading until done
              });
            }

            read();
          }
        });
      }).catch((err) => { // this catches an issue with in the ^ .then
        subscriber.error(err);
        return subscriber.complete();
      });
    });
  }
};
