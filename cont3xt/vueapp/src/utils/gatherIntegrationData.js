'use strict';

/**
 * Gathers integration data for a single indicator into an object of shape: {[integrationName]: data}
 *
 * @param {object} data the full results object returned by cont3xt search
 * @param {string} itype the itype of the indicator to get integration data for
 * @param {string} query the indicator to get integration data for
 * @returns {object} Returns an object of shape {[integrationName]: data} for the desired indicator
 */
export const gatherIntegrationData = (data, itype, query) => {
  const iTypeStub = data?.[itype] || {};
  const integrationPairs = Object.entries(iTypeStub)
    .filter(([key, _]) => key !== '_query')
    .map(([integrationName, dataEntryArray]) => [integrationName, dataEntryArray?.find(dataEntry => dataEntry._query === query)?.data])
    .filter(([_, val]) => val != null);

  return Object.fromEntries(integrationPairs);
};
