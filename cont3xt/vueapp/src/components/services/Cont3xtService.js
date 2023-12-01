import dr from 'defang-refang';

import Observable from '@/utils/Observable';
import store from '@/store';
import setReqHeaders from '../../../../../common/vueapp/setReqHeaders';

export default {
  /**
   * Decodes an array of 8-bit unsigned integers into text
   * @param {Array} arr - Uint8Array
   * @returns {String} - The text represented by the Uint8Array array
   */
  decoder (arr) {
    const dec = new TextDecoder('utf-8');
    return dec.decode(arr);
  },

  /**
   * Parses JSON and sends the chunk to the subscriber.
   * Also sends any errors to the subscriber.
   * Logs JSON parsing errors to the console with the offending JSON.
   * @param {Object} subscriber - The subscriber to send chunks
   * @param {String} chunk - The stringified JSON to parse and send to the subscriber
   */
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
   * Fetches integration data from the server given a search term.
   * Watches for a stream of data then reads and parses each chunk.
   * Sends chunks back to the subscriber of the search.
   * @param {String} searchTerm - The query to search the integrations for
   * @param {Boolean} skipCache - Whether to use the cached result or fetch it new
   * @param {Boolean} skipChildren - Whether to process the children queries
   * @param {string[]} tags - Tags applied at the time of search
   * @param {string | undefined} viewId - The ID of the view at the time of search (if any)
   * @returns {Observable} - The observable object to subscribe to updates
   */
  search ({ searchTerm, skipCache, skipChildren, tags, viewId }) {
    return new Observable((subscriber) => {
      searchTerm = dr.refang(searchTerm.trim());

      if (!searchTerm) { // nothing to do
        return subscriber.complete();
      }

      // NOTE: this assumes that the client has already fetched the integrations
      const doIntegrations = [];
      for (const integration of store.state.selectedIntegrations) {
        if (store.state.integrations[integration] && store.state.integrations[integration].doable) {
          doIntegrations.push(integration);
        }
      }

      fetch('api/integration/search', {
        method: 'POST',
        headers: setReqHeaders({ 'Content-Type': 'application/json' }),
        body: JSON.stringify({ query: searchTerm, skipCache, skipChildren, doIntegrations, tags, viewId })
      }).then((response) => {
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
      }).catch((err) => { // this catches an issue within the ^ .then
        subscriber.error(err);
        return subscriber.complete();
      });
    });
  },

  /**
   * Fetches the list of integrations that are configured.
   * @returns {Promise} - The promise that either resolves the request or rejects in error
   */
  getIntegrations () {
    return new Promise((resolve, reject) => {
      fetch('api/integration').then((response) => {
        if (!response.ok) { // test for bad response code
          throw new Error(response.statusText);
        }
        return response.json();
      }).then((response) => {
        store.commit('SET_INTEGRATIONS', response.integrations);
        if (!store.state.selectedIntegrations) {
          // if integrations have not been initialized, set them!
          store.commit('SET_SELECTED_INTEGRATIONS', Object.keys(response.integrations));
        }
        return resolve(response.integrations);
      }).catch((err) => { // this catches an issue within the ^ .then
        store.commit('SET_INTEGRATIONS_ERROR', err);
        return reject(err);
      });
    });
  },

  /**
   * Refreshes data for an integration card
   * @param {Cont3xtIndicator} indicator - Contains the query/itype shown in the integration card
   * @param {String} source - The integration that was displayed in the card
   * @returns {Promise} - The promise that either resolves the request or rejects in error
   */
  refresh ({ indicator, source }) {
    return new Promise((resolve, reject) => {
      const { itype, query } = indicator;

      fetch(`api/integration/${itype}/${source}/search`, {
        method: 'POST',
        headers: setReqHeaders({ 'Content-Type': 'application/json' }),
        body: JSON.stringify({ query })
      }).then((response) => {
        if (!response.ok) { // test for bad response code
          throw new Error(response.statusText);
        }
        return response.json();
      }).then((response) => {
        return resolve(response);
      }).catch((err) => { // this catches an issue within the ^ .then
        return reject(err);
      });
    });
  },

  /**
   * Retrieves integration stats
   * @returns {Promise} - The promise that either resolves the request or rejects in error
   */
  getStats () {
    return new Promise((resolve, reject) => {
      fetch('api/integration/stats').then((response) => {
        if (!response.ok) { // test for bad response code
          throw new Error(response.statusText);
        }
        return response.json();
      }).then((response) => {
        return resolve(response);
      }).catch((err) => { // this catches an issue within the ^ .then
        return reject(err);
      });
    });
  }
};
