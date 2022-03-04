/**
 * Gets a cookie given its name and returns undefined if it can't be found
 * @param {String} cookieName - The name of the cookie to retrieve
 * @returns {String} cookie - The value of the cookie or undefined if it can't be found
 */
function getCookie (cookieName) {
  return document.cookie.match('(^|;)\\s*' + cookieName + '\\s*=\\s*([^;]+)')?.pop() || undefined;
}

/**
 * Sets the xsrf headers for cont3xt and arkime fetch requests
 * If supplied existing headers, combines those with the new cookie headers
 * as well as an 'X-Requested-With': 'XMLHttpRequest' header
 * @param {Object} headers - The existing headers to combine with the new xsrf headers
 * @returns {object} headers - The http headers object including the supplied
 *                             headers, 'X-Requested-With', and the cookies
 */
export default function setReqHeaders (headers) {
  const combinedHeaders = {
    ...headers,
    'X-Requested-With': 'XMLHttpRequest'
  };

  const arkimeCookie = getCookie('ARKIME-COOKIE');
  const cont3xtCookie = getCookie('CONT3XT-COOKIE');

  if (cont3xtCookie) {
    combinedHeaders['x-cont3xt-cookie'] = cont3xtCookie;
  }

  if (arkimeCookie) {
    combinedHeaders['x-arkime-cookie'] = arkimeCookie;
  }

  return combinedHeaders;
}
