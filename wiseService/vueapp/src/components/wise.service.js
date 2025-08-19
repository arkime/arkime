import { fetchWrapper } from '@real_common/fetchWrapper.js';

export default {
  getSources: async function () {
    return await fetchWrapper({ url: 'sources' });
  },
  getTypes: async function (source) {
    return await fetchWrapper({ url: source ? 'types/' + source : 'types' });
  },
  getResourceStats: async function (query) {
    return await fetchWrapper({ url: 'stats', params: query });
  },
  getConfigDefs: async function () {
    return await fetchWrapper({ url: 'config/defs' });
  },
  getCurrConfig: async function () {
    return await fetchWrapper({ url: 'config/get' });
  },
  getSourceFile: async function (sourceName) {
    return await fetchWrapper({ url: 'source/' + sourceName + '/get' });
  },
  saveSourceFile: async function (sourceName, data, configCode) {
    // TODO: new file saving
    return await fetchWrapper({ url: 'source/' + sourceName + '/put', method: 'PUT', data: { raw: data, configCode } });
  },
  getSourceDisplay: async function (sourceName) {
    return await fetchWrapper({ url: 'dump/' + sourceName, headers: { 'Content-Type': 'text/plain' } });
  },
  saveCurrConfig: async function (config, configCode) {
    return await fetchWrapper({ url: 'config/save', method: 'PUT', data: { config, configCode } });
  },
  search: async function (source, type, value) {
    const url = ((source ? source.replaceAll(':', '%3A') + '/' : '') + type + '/' + value.replaceAll('/', '%2F'));
    return await fetchWrapper({ url });
  }
};
