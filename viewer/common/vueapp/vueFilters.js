// NOTE: these are no longer 'filters', as the filter syntax was removed in Vue 3.
// So now these are just assorted string formatting functions :)
// TODO this is duplicated between cont3xt and viewer, should be moved to top level common
import moment from 'moment-timezone';

/**
 * Rounds a number using Math.round
 *
 * @example
 * '{{ round(1234.56, 0) }}'
 *
 * @param {number} value  The number to round
 * @param {int} decimals  The number of decimals to preserve, default = 0
 * @returns {decimal}     The number rounded to the requested number of decimal places
 */
export const round = function (value, decimals) {
  if (isNaN(value)) { return 0; }

  if (!value) { value = 0; }

  if (!decimals) { decimals = 0; }

  return Math.round(value * Math.pow(10, decimals)) / Math.pow(10, decimals);
};

/**
 * Adds commas to a number so it's easier to read
 *
 * @example
 * '{{ commaString(123456789) }}'
 *
 * @param {int} input The number to add commas to
 * @returns {string}  The number string with commas
 */
export const commaString = function (input) {
  if (isNaN(input)) { return 0; }

  const parts = input.toString().split('.');
  parts[0] = parts[0].replace(/\B(?=(\d{3})+(?!\d))/g, ',');
  return parts.join('.');
};

/**
 * Rounds a number then adds commas so it's easier to read
 *
 * @example
 * '{{ roundCommaString(123456789) }}'
 *
 * @param {int} input     The number to add commas to
 * @param {int} decimals  The number of decimals to preserve, default = 0
 * @returns {string}      The number string with commas
 */
export const roundCommaString = function (input, decimals) {
  if (isNaN(input)) { return 0; }

  if (!decimals) { decimals = 0; }
  return commaString(round(input, decimals).toFixed(decimals));
};

/**
 * Modifies a number to display the <=4 char human readable version of bytes
 * Modified http://stackoverflow.com/questions/10420352/converting-file-size-in-bytes-to-human-readable
 *
 * @example
 * '{{ humanReadableNumber(1524680821) }}'
 *
 * @param {int} num   The number to make human readable
 * @returns {string}  The <=4 char human readable number
 */
export const humanReadableNumber = function (num) {
  if (isNaN(num)) { return '0 '; }

  let i = 0;
  const units = [' ', 'k', 'M', 'G', 'T', 'P', 'E', 'Z', 'Y'];
  while (num >= 1000) {
    num = num / 1000;
    i++;
  }

  if (i === 0 || num >= 10) {
    return num.toFixed(0) + units[i];
  } else {
    return num.toFixed(1) + units[i];
  }
};

/**
 * Parses date to string and applies the selected timezone
 *
 * @example
 * '{{ timezoneDateString(1524680821, "local", false) }}'
 *
 * @param {int} ms           The time in milliseconds from epoch
 * @param {string} timezone  The timezone to use ('gmt', 'local', or 'localtz'), default = 'local'
 * @returns {string}         The date formatted and converted to the requested timezone
 */
export const timezoneDateString = function (ms, timezone, showMs) {
  if (isNaN(ms)) { return 'Invalid date'; }

  let format = showMs ? 'YYYY/MM/DD HH:mm:ss.SSS z' : 'YYYY/MM/DD HH:mm:ss z';

  if (timezone === 'gmt') {
    return moment.tz(ms, 'utc').format(format);
  } else if (timezone === 'localtz') {
    return moment.tz(ms, Intl.DateTimeFormat().resolvedOptions().timeZone).format(format);
  }

  format = showMs ? 'YYYY/MM/DD HH:mm:ss.SSS' : 'YYYY/MM/DD HH:mm:ss';
  return moment(ms).format(format);
};

// used in parseSeconds to determine the unit of relative time
function str2format (str) {
  if (str.match(/^(s|sec|secs|second|seconds)$/)) {
    return 'seconds';
  } else if (str.match(/^(m|min|mins|minute|minutes)$/)) {
    return 'minutes';
  } else if (str.match(/^(h|hr|hrs|hour|hours)$/)) {
    return 'hours';
  } else if (str.match(/^(d|day|days)$/)) {
    return 'days';
  } else if (str.match(/^(w|week|weeks)\d*$/)) {
    return 'weeks';
  } else if (str.match(/^(M|mon|mons|month|months)$/)) {
    return 'months';
  } else if (str.match(/^(q|qtr|qtrs|quarter|quarters)$/)) {
    return 'quarters';
  } else if (str.match(/^(y|yr|yrs|year|years)$/)) {
    return 'years';
  }

  return undefined;
}

/**
 * Parses the current time to seconds based on a relative time
 * (+/- seconds/minutes/hours/days/weeks/months/quarters/years)
 *
 * @example
 * '{{ parseSeconds(-5h) }}'
 *
 * @param {string} str The relative time string
 */
export const parseSeconds = function (str) {
  if (str === '' || str === 'now') {
    return moment().unix();
  }

  let m, n;
  if ((m = str.match(/^([+-])(\d*)([a-z]*)([@]*)([a-z0-9]*)/))) {
    const d = moment();
    const format = str2format(m[3]);
    const snap = str2format(m[5]);

    if (m[2] === '') {
      m[2] = 1;
    }

    if (snap) {
      d.startOf(snap);
      if ((n = m[5].match(/^(w|week|weeks)(\d+)$/))) {
        d.day(n[2]);
      }
    }

    d.add((m[1] === '-' ? -1 : 1) * m[2], format);
    return d.unix();
  }

  if ((m = str.match(/^@([a-z0-9]+)/))) {
    const d = moment();
    const snap = str2format(m[1]);

    d.startOf(snap);
    if ((n = m[1].match(/^(w|week|weeks)(\d+)$/))) {
      d.day(n[2]);
    }
    return d.unix();
  }

  return moment(str, ['YYYY/MM/DDTHH:mm:ss', 'YYYY/MM/DDTHH:mm:ssZ', moment.ISO_8601]).unix();
};

/**
 * remove "role:" from user defined roles and mark them as userDefined
 * ['role:amazingrole', 'arkimeUser'] ->
   [
     { text: 'amazingrole', roleId: 'role:amazingrole', userDefined: true},
     { text: 'arkimeUser', roleId: 'arkimeUser', userDefined: false }
   ]
 *
 * @example
 * '{{ parseRoles(["role:amazingrole", "arkimeUser"]) }}'
 *
 * @param {array} rolesStrs - the list of role strings to parse
 * @returns {array} roleObjs - the role objects parsed from basic role strings
 */
export const parseRoles = function (roleStrs) {
  const roles = [];

  for (let role of roleStrs) {
    let userDefined = false;
    const roleId = role;
    if (role.startsWith('role:')) {
      role = role.slice(5);
      userDefined = true;
    }
    role = { text: role, value: roleId, userDefined };
    roles.push(role);
  }

  return roles;
};

/**
 * Role Search
 *
 * @example
 * '{{ searchRoles(roles, "role2") }}'
 *
 * @param {Array} roles The role objects to search
 * @param {string} term The search term to search roles for
 * @returns {Array}     The roles that match the search term
 */
export const searchRoles = function (roles, term) {
  if (!term) { return roles; }
  return roles.filter((role) => {
    return role.text.toLowerCase().includes(term.toLowerCase());
  });
};

// TODO the below functions are specific to viewer
/**
 * Modifies a number to display the <=4 char human readable version of bytes
 * Modified http://stackoverflow.com/questions/10420352/converting-file-size-in-bytes-to-human-readable
 *
 * @example
 * '{{ humanReadableBytes(1524680821)  }}'
 *
 * @param {int} fileSizeInBytes The number to make human readable
 * @returns {string}            The <=4 char human readable number
 */
export const humanReadableBytes = function (fileSizeInBytes) {
  fileSizeInBytes = parseInt(fileSizeInBytes);

  if (isNaN(fileSizeInBytes)) { return '0'; }

  let i = 0;
  const byteUnits = ['Bi', 'Ki', 'Mi', 'Gi', 'Ti', 'Pi', 'Ei', 'Zi', 'Yi'];
  while (fileSizeInBytes >= 1000) {
    fileSizeInBytes = fileSizeInBytes / 1024;
    i++;
  }

  if (i === 0 || fileSizeInBytes >= 10) {
    return fileSizeInBytes.toFixed(0) + byteUnits[i];
  } else {
    return fileSizeInBytes.toFixed(1) + byteUnits[i];
  }
};

/**
 * Modifies a number to display the <=4 char human readable version of bits
 * Modified http://stackoverflow.com/questions/10420352/converting-file-size-in-bytes-to-human-readable
 *
 * @example
 * '{{ humanReadableBits(1524680821) }}'=
 *
 * @param {int} fileSizeInBits The number to make human readable
 * @returns {string}           The <=4 char human readable number
 */
export const humanReadableBits = function (fileSizeInBits) {
  fileSizeInBits = parseInt(fileSizeInBits);

  if (isNaN(fileSizeInBits)) { return '0'; }

  let i = 0;
  const bitUnits = ['', 'K', 'M', 'G', 'T', 'P', 'E', 'Z', 'Y'];
  while (fileSizeInBits >= 1024) {
    fileSizeInBits = fileSizeInBits / 1024;
    i++;
  }

  if (i === 0 || fileSizeInBits >= 10) {
    return fileSizeInBits.toFixed(0) + bitUnits[i];
  } else {
    return fileSizeInBits.toFixed(1) + bitUnits[i];
  }
};

/**
 * Turns milliseconds into a human readable time range
 *
 * @example
 * '{{ readableTime(1524680821790) }}'
 *
 * @param {int} ms    The time in ms from epoch
 * @returns {string}  The human readable time range
 *                    Output example: 1 day 10:42:01
 */
export const readableTime = function (ms) {
  if (isNaN(ms)) { return '?'; }

  const seconds = parseInt((ms / 1000) % 60);
  const minutes = parseInt((ms / (1000 * 60)) % 60);
  const hours = parseInt((ms / (1000 * 60 * 60)) % 24);
  const days = parseInt((ms / (1000 * 60 * 60 * 24)));

  let result = '';

  if (days) {
    result += days + ' day';
    if (days > 1) {
      result += 's ';
    } else {
      result += ' ';
    }
  }

  if (hours || minutes || seconds) {
    result += (hours < 10) ? '0' + hours : hours;
    result += ':';
    result += (minutes < 10) ? '0' + minutes : minutes;
    result += ':';
    result += (seconds < 10) ? '0' + seconds : seconds;
  }

  return result || '0';
};

/**
 * Turns milliseconds into a human readable time range
 *
 * @example
 * '{{ readableTimeCompact(1524680821790) }}'
 *
 * @param {int} ms    The time in ms from epoch
 * @returns {string}  The human readable time range
 *                    Output example: 1 day 10:42:01
 */
export const readableTimeCompact = function (ms) {
  if (isNaN(ms)) { return '?'; }

  const hours = parseInt((ms / (1000 * 60 * 60)) % 24);
  const days = parseInt((ms / (1000 * 60 * 60 * 24)));

  let result = '';

  if (days) {
    result += days + 'd ';
  }
  result += hours + 'h';
  return result;
};

/**
 * Searches fields for a term
 * Looks for the term in field friendlyName, exp, and aliases
 *
 * @example
 * '{{ searchFields('test', fields, true) }}'
 *
 * @param {string} searchTerm       The string to search for within the fields
 * @param {array} fields            The list of fields to search
 * @param {boolean} excludeTokens   Whether to exclude token fields
 * @param {boolean} excludeFilename Whether to exclude the filename field
 * @param {boolean} excludeInfo     Whether to exclude the special info "field"
 * @returns {array}                 An array of fields that match the search term
 */
export const searchFields = function (searchTerm, fields, excludeTokens, excludeFilename, excludeInfo) {
  if (!searchTerm) { searchTerm = ''; }
  return fields.filter((field) => {
    if (field.regex !== undefined || field.noFacet === 'true') {
      return false;
    }

    if (excludeTokens && field.type && field.type.includes('textfield')) {
      return false;
    }

    if (excludeFilename && field.type && field.type === 'fileand') {
      return false;
    }

    if (excludeInfo && field.exp === 'info') {
      return false;
    }

    searchTerm = searchTerm.toLowerCase();
    return field.friendlyName.toLowerCase().includes(searchTerm) ||
      field.exp.toLowerCase().includes(searchTerm) ||
      (field.aliases && field.aliases.some(item => {
        return item.toLowerCase().includes(searchTerm);
      }));
  });
};

/**
 * Builds an expression for search.
 * Stringifies necessary values and escapes necessary characters
 *
 * @example
 * '{{ buildExpression('ip.dst', '10.0.0.1', '==') }}'
 *
 * @param {string} field  The field name
 * @param {string} value  The field value
 * @param {string} op     The relational operator
 * @returns {string}      The fully built expression
 */
export const buildExpression = function (field, value, op) {
  // for values required to be strings in the search expression
  /* eslint-disable no-useless-escape */
  const needQuotes = (value !== 'EXISTS!' && !(value.startsWith('[') && value.endsWith(']')) &&
    /[^-+a-zA-Z0-9_.@:*?/,]+/.test(value)) ||
    (value.startsWith('/') && value.endsWith('/'));

  // escape unescaped quotes
  value = value.toString().replace(/\\([\s\S])|(")/g, '\\$1$2');
  if (needQuotes) { value = `"${value}"`; }

  return `${field} ${op} ${value}`;
};

/**
 * Searches cluster for a term
 * Looks for the term in a list of cluster
 *
 * @example
 * '{{ searchCluster('ES1', ['ES1', 'ES2', 'ES3']) }}'
 *
 * @param {string} searchTerm The string to search for within the fields
 * @param {array} clusters    The list of cluster to search
 * @returns {array}           An array of cluster that match the search term
 */
export const searchCluster = function (searchTerm, clusters) {
  if (!searchTerm) { searchTerm = ''; }
  return clusters.filter((cluster) => {
    searchTerm = searchTerm.toLowerCase();
    return cluster.toLowerCase().includes(searchTerm);
  });
};

/**
 * Parses ipv6
 *
 * @example
 * '{{ extractIPv6String(ipv6) }}'
 *
 * @param {int} ipv6  The ipv6 value
 * @returns {string}  The human understandable ipv6 string
 */
export const extractIPv6String = function (ipv6) {
  if (!ipv6) { return ''; }

  ipv6 = ipv6.toString();

  let ip = ipv6.match(/.{1,4}/g).join(':').replace(/:0{1,3}/g, ':').replace(/^0000:/, '0:');
  [/(^|:)0:0:0:0:0:0:0:0($|:)/,
    /(^|:)0:0:0:0:0:0:0($|:)/,
    /(^|:)0:0:0:0:0:0($|:)/,
    /(^|:)0:0:0:0:0($|:)/,
    /(^|:)0:0:0:0($|:)/,
    /(^|:)0:0:0($|:)/,
    /(^|:)0:0($|:)/]
    .every(function (re) {
      if (ipv6.match(re)) {
        ip = ipv6.replace(re, '::');
        return false;
      }
      return true;
    });

  return ip;
};

/**
 * Displays the protocol string instead of number code
 *
 * @example
 * '{{ protocol(1) }}'
 *
 * @param {int} protocolCode  The protocol code
 * @returns {string}          The human understandable protocol string
 */
export const protocol = function (protocolCode) {
  const lookup = { 1: 'icmp', 2: 'igmp', 6: 'tcp', 17: 'udp', 47: 'gre', 50: 'esp', 58: 'icmp6', 89: 'ospf', 103: 'pim', 132: 'sctp' };

  return lookup[protocolCode] || protocolCode;
};
