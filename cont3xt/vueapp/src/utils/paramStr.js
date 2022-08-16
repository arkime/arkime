/**
 * Converts an object into an url-safe query parameter string
 * @param {Object} queryParamObj the object whose fields (of type string, boolean, or number) are converted into query parameters
 * @returns {string} ex. '?q1=value1', '?q1=value1&q2=value2', '?param_with_special_characters=cHVycGxlLm9yZw%3D%3D'
 */
export const paramStr = (queryParamObj) => {
  // no parameters, return empty
  if (!Object.keys(queryParamObj)) { return ''; }
  // form query param string
  return '?' + Object.entries(queryParamObj)
    .filter(([_, value]) => typeof value === 'string' || typeof value === 'number' || typeof value === 'boolean')
    .map(([key, value]) => `${encodeURIComponent(key)}=${encodeURIComponent('' + value)}`)
    .join('&');
};
