/* lexical grammar */
%lex

%options flex
%%

\s+                        /* skip whitespace */
\"(?:\\?.)*?\"             return 'QUOTEDSTR'
\/(?:\\?.)*?\/             return 'REGEXSTR'
[-+a-zA-Z0-9_.@:*?/$]+      return 'STR'
\[[^\]\\]*(?:\\.[^\]\\]*)*\] return 'LIST'
\][^\[\\]*(?:\\.[^\[\\]*)*\[ return 'LIST'
"EXISTS!"                  return "EXISTS"
"<="                       return 'lte'
"<"                        return 'lt'
">="                       return 'gte'
">"                        return 'gt'
"!="                       return '!='
"=="                       return '=='
"="                        return '=='
"||"                       return '||'
"|"                        return '||'
"&&"                       return '&&'
"&"                        return '&&'
"("                        return '('
")"                        return ')'
"!"                        return '!'
<<EOF>>                    return 'EOF'
.                          return 'INVALID'

/lex

/* operator associations and precedence */

%left '!'
%left '<' '<=' '>' '>=' '==' '!='
%left '||'
%left '&&'
%left UMINUS

%start expressions

%% /* language grammar */

expressions
    : e EOF
        { return $1; }
    ;

OP  : lt   {$$ = 'lt'}
    | lte  {$$ = 'lte'}
    | gt   {$$ = 'gt'}
    | gte  {$$ = 'gte'}
    | '==' {$$ = 'eq'}
    | '!=' {$$ = 'ne'}
    ;

VALUE : STR
      | QUOTEDSTR
      | REGEXSTR
      | LIST
      ;


e
    : e '&&' e
        {$$ = {bool: {filter: [$1, $3]}};}
    | e '||' e
        {$$ = {bool: {should: [$1, $3]}};}
    | '!' e %prec UMINUS
        {$$ = {bool: {must_not: $2}};}
    | '-' e %prec UMINUS
        {$$ = -$2;}
    | '(' e ')'
        {$$ = $2;}
    | STR '==' EXISTS
        {$$ = formatExists(yy, $1, "eq");}
    | STR '!=' EXISTS
        {$$ = formatExists(yy, $1, "ne");}
    | STR OP VALUE
        { $$ = formatQuery(yy, $1, $2, $3);
          //console.log(util.inspect($$, false, 50));
        }
    ;
%%
const util = require('util');
const moment = require('moment');

/* Given a field name, if prefixed with 'db:' return dbFieldsMap entry (i.e., looked up according to
 * the OpenSearch/Elasticsearch field name); otherwise return fieldsMap entry (see #1461)
 */
function getFieldInfo (yy, field) {
  let info = null;

  if (field.startsWith('db:')) {
    const dbField = field.substring(3);
    if (yy.dbFieldsMap[dbField]) {
      info = yy.dbFieldsMap[dbField];
    }
  } else if (yy.fieldsMap[field]) {
    info = yy.fieldsMap[field];
  }

  // console.log('getFieldInfo', field, info);
  return info;
}

/* Build a list of all the field infos for ip field types.
 * Can specify if a port field needs to be available for the type or not
 */
function getIpInfoList (yy, needPort) {
  const ors = [];
  const completed = {};

  for (const field in yy.fieldsMap) {
    const info = yy.fieldsMap[field];

    // If ip itself or not an ip field stop
    if (field === 'ip' || info.type !== 'ip') { continue; }

    // Already completed
    if (completed[info.dbField]) { continue; }
    completed[info.dbField] = 1;

    // If port specified then skip ips without ports
    if (needPort && !info.portField) {
      continue;
    }

    if (info.requiredRight && yy[info.requiredRight] !== true) {
      continue;
    }

    ors.push(info);
  }

  return ors.sort((a, b) => { return a.exp.localeCompare(b.exp); });
}

function getRegexInfoList (yy, info) {
  const regex = new RegExp(info.regex);
  const ors = [];
  const completed = [];
  for (const f in yy.fieldsMap) {
    if (f.match(regex) && !completed[yy.fieldsMap[f].dbField]) {
      if (yy.fieldsMap[f].requiredRight && yy[yy.fieldsMap[f].requiredRight] !== true) {
        continue;
      }
      ors.push(yy.fieldsMap[f]);
      completed[yy.fieldsMap[f].dbField] = 1;
    }
  }

  return ors.sort((a, b) => { return a.exp.localeCompare(b.exp); });
}

/* Do all the magic around ip field parsing.
 * Supports many formats such as
 * ip
 * ip/cidr
 * ip1 = ip1.0.0.0/24
 * ip1.ip2 = ip1.ip2.0.0/16
 * ip1.ip2.ip3 = ip1.ip2.ip3.0/8
 * All of the above with a :port(v4) or .port(v6) at the end
 * Arrays of all of the above
 */
function parseIpPort (yy, field, ipPortStr) {
  const dbField = getFieldInfo(yy, field).dbField;

  // Have just a single Ip, create obj for it
  function parseSingleIp (exp, singleDbField, singleIp, singlePort) {
    let singleObj;

    if (typeof (singlePort) === 'string' && singlePort.match(/[^0-9]/)) {
      throw singlePort + ' not a valid singlePort';
    }

    if (singleIp !== undefined) {
      const colon = singleIp.indexOf(':');
      if ((colon === -1 && singleIp.match(/[^.0-9/]/)) || // IP4
          (colon !== -1 && singleIp.match(/[^a-fA-F:0-9/]/)) || // IP6
          singleIp.match(/\/.*[^0-9]/)) { // CIDR
        throw singleIp + ' not a valid ip';
      }

      singleObj = { term: {} };
      singleObj.term[singleDbField] = singleIp;
    }

    if (singlePort !== -1) {
      const expInfo = getFieldInfo(yy, exp);
      if (expInfo.portField) {
        singleObj = { bool: { filter: [singleObj, { term: {} }] } };
        singleObj.bool.filter[1].term[expInfo.portField] = singlePort;
      } else {
        throw exp + " doesn't support port";
      }

      if (singleIp === undefined) {
        singleObj = singleObj.bool.filter[1];
      }
    }

    return singleObj;
  }

  // Special case of ip=
  function parseAllIp (allIp, allPort) {
    const infos = getIpInfoList(yy, allPort !== -1);

    const ors = [];
    for (const info of infos) {
      const allObj = parseSingleIp(info.exp, info.dbField, allIp, allPort);
      if (allObj) {
        ors.push(allObj);
      }
    }

    return { bool: { should: ors } };
  }

  let obj;

  ipPortStr = ipPortStr.trim();

  // We really have a list of them
  if (isArrayFull(ipPortStr)) {
    const list = ListToArrayMap(ipPortStr, str => parseIpPort(yy, field, str));

    // Optimize 1 item in array
    if (list.length === 1) {
      return list[0]
    }

    if (isArrayAND(ipPortStr)) {
      obj = { bool: { filter: list } };
    } else {
      obj = { bool: { should: list } };
    }
    return obj;
  }

  // Support ':80' and '.80'
  if ((ipPortStr[0] === ':' && ipPortStr[0] !== ':') ||
      (ipPortStr[0] === '.')) {
    if (dbField !== 'ipall') {
      return parseSingleIp(field, dbField, undefined, +ipPortStr.slice(1));
    } else {
      return parseAllIp(undefined, +ipPortStr.slice(1));
    }
  }

  // Support ip4: '10.10.10.10' '10.10.10/16:80' '10.10.10:80' '10.10.10/16'
  // Support ip6: '1::2' '1::2/16.80' '1::2.80' '1::2/16'
  let ip;
  let port = -1;
  const colons = ipPortStr.split(':');

  if (ipPortStr.includes('::')) { // Double colon is ip6 with 0 filled
    // Everything after . is port
    const dots = ipPortStr.split('.');
    if (dots.length > 1 && dots[1] !== '') {
      port = dots[1];
    }
    // Everything before . is ip and slash
    ip = dots[0];
  } else if (colons.length > 2) { // More than 1 colon is ip6 
    // Everything after . is port
    const dots = ipPortStr.split('.');
    if (dots.length > 1 && dots[1] !== '') {
      port = dots[1];
    }

    const slash = dots[0].split('/');
    const colons2 = slash[0].split(':');

    // If the last one is empty just pretend : isn't there, for auto complete
    if (colons2[colons2.length - 1] === '') {
      colons2.length--;
    }

    if (slash[1] === undefined) {
      console.log(colons2.length, colons2, colons2.length, 16*colons2.length);
      slash[1] = `${16*colons2.length}`;
    }

    if (colons2.length < 8) {
      ip = colons2.join(':') + '::';
    } else {
      ip = colons2.join(':');
    }


    // Add the slash back to the ip
    if (slash[1] && slash[1] !== '128') {
      ip = `${ip}/${slash[1]}`;
    }
  } else {
    // everything after : is port
    if (colons.length > 1 && colons[1] !== '') {
      port = colons[1];
    }

    // Have to do extra because we allow shorthand for /8, /16, /24
    const slash = colons[0].split('/');
    const dots = slash[0].split('.');

    // If the last one is empty just pretend . isn't there, for auto complete
    if (dots[dots.length - 1] === '') {
      dots.length--;
    }

    switch (dots.length) {
    case 4:
      ip = `${dots[0]}.${dots[1]}.${dots[2]}.${dots[3]}`;
      break;
    case 3:
      ip = `${dots[0]}.${dots[1]}.${dots[2]}.0`;
      if (slash[1] === undefined) { slash[1] = '24'; }
      break;
    case 2:
      ip = `${dots[0]}.${dots[1]}.0.0`;
      if (slash[1] === undefined) { slash[1] = '16'; }
      break;
    case 1:
      if (dots[0].length > 0) {
        ip = `${dots[0]}.0.0.0`;
        if (slash[1] === undefined) { slash[1] = '8'; }
      }
      break;
    }

    // Add the slash back to the ip
    if (slash[1] && slash[1] !== '32') {
      ip = `${ip}/${slash[1]}`;
    }
  }

  if (dbField !== 'ipall') {
    return parseSingleIp(field, dbField, ip, port);
  } else {
    return parseAllIp(ip, port);
  }
}

function stripQuotes (str) {
  if (str[0] === '"') {
    str = str.substring(1, str.length - 1);
  }
  return str;
}

function formatExists (yy, field, op) {
  const info = getFieldInfo(yy, field);
  if (!info) { throw 'Unknown field ' + field; }

  if (info.requiredRight && yy[info.requiredRight] !== true) {
    throw field + ' - permission denied';
  }

  if (info.regex) {
    const regex = new RegExp(info.regex);
    const obj = [];
    const completed = [];
    for (const f in yy.fieldsMap) {
      if (f.match(regex) && !completed[yy.fieldsMap[f].dbField]) {
        if (yy.fieldsMap[f].requiredRight && yy[yy.fieldsMap[f].requiredRight] !== true) {
          continue;
        }
        obj.push(formatExists(yy, f, 'eq'));
        completed[yy.fieldsMap[f].dbField] = 1;
      }
    }
    if (op === 'ne') {
      return { bool: { must_not: obj } };
    } else {
      return { bool: { should: obj } };
    }
  }

  if (op === 'ne') {
    return { bool: { must_not: { exists: { field: field2Raw(yy, field) } } } };
  }

  return { exists: { field: field2Raw(yy, field) } };
}

function formatQuery (yy, field, op, value) {
  // console.log("field", field, "op", op, "value", value);
  // console.log("yy", util.inspect(yy, false, 50));
  if (value[0] === '/' && value[value.length - 1] === '/') {
    checkRegex(value);
  }

  const info = getFieldInfo(yy, field);
  if (!info) { throw 'Unknown field ' + field; }

  if (info.requiredRight && yy[info.requiredRight] !== true) {
    throw field + ' - permission denied';
  }

  const nonShortcuts = ListToArray(value, true);

  const shortcutsObj = formatShortcutsQuery(yy, field, op, value);
  if (nonShortcuts.length === 0) { return shortcutsObj; }

  const normalObj = formatNormalQuery(yy, field, op, value);
  if (!shortcutsObj) { return normalObj; }

  if (op === 'eq') { return { bool: { should: [normalObj, shortcutsObj] } }; } else { return { bool: { must_not: [normalObj, shortcutsObj] } }; }
}

function formatShortcutsQuery (yy, field, op, value, shortcutParent) {
  const shortcuts = ListToArrayShortcuts(yy, value);
  if (shortcuts.length === 0) { return undefined; }

  if (isArrayAND(value)) { throw value + ' - AND array not supported with shortcuts'; }

  let obj;
  const info = getFieldInfo(yy, field);

  if (op !== 'eq' && op !== 'ne') {
    throw 'Shortcuts only support == and !=';
  }

  obj = { bool: {} };

  let operation = 'should';
  if (op === 'ne') {
    operation = 'must_not';
  }
  obj.bool[operation] = [];

  shortcuts.forEach(function (shortcut) {
    shortcut = value = shortcut.substr(1); /* remove $ */
    if (!yy.shortcuts || !yy.shortcuts[shortcut]) {
      throw shortcut + ' - Shortcut not found';
    }

    shortcut = yy.shortcuts[shortcut];

    const type = info.type2 || info.type;
    const shortcutType = yy.shortcutTypeMap[type];

    if (!shortcutType) {
      throw 'Unsupported field type: ' + type;
    }

    if (!shortcut._source[shortcutType]) {
      throw 'shortcut must be of type ' + shortcutType;
    }

    const terms = {};

    switch (type) {
    case 'ip':
      if (field === 'ip') {
        const infos = getIpInfoList(yy, false);
        for (const ipInfo of infos) {
          const newObj = formatShortcutsQuery(yy, ipInfo.exp, op, '$' + value, obj);
          if (newObj) {
            obj.bool[operation].concat(newObj);
          }
        }
      } else {
        terms[info.dbField] = {
          index: `${yy.prefix}lookups`,
          id: shortcut._id,
          path: 'ip'
        };
        if (shortcutParent) {
          obj = shortcutParent;
        }
        obj.bool[operation].push({ terms });
      }
      break;
    case 'integer':
      terms[info.dbField] = {
        index: `${yy.prefix}lookups`,
        id: shortcut._id,
        path: 'number'
      };
      obj.bool[operation].push({ terms });
      break;
    case 'lotermfield':
    case 'lotextfield':
    case 'termfield':
    case 'textfield':
    case 'uptermfield':
    case 'uptextfield':
      if (info.regex) {
        const infos = getRegexInfoList(yy, info);
        for (const i of infos) {
          const terms = {};
          terms[i.dbField] = {
            index: `${yy.prefix}lookups`,
            id: shortcut._id,
            path: 'string'
          };
          obj.bool[operation].push({ terms });
        }
      } else {
        terms[info.dbField] = {
          index: `${yy.prefix}lookups`,
          id: shortcut._id,
          path: 'string'
        };
        obj.bool[operation].push({ terms });
      }
      break;
    default:
      throw 'Unsupported field type: ' + type;
    }
  });

  if (obj?.bool?.should?.length === 1) {
    obj = obj.bool.should[0];
  }

  return obj;
}

function formatNormalQuery (yy, field, op, value) {
  let obj;
  const info = getFieldInfo(yy, field);

  if (info.regex) {
    const infos = getRegexInfoList(yy, info);
    obj = [];
    const completed = [];
    for (const i of infos) {
      obj.push(formatQuery(yy, i.exp, (op === 'ne' ? 'eq' : op), value));
    }

    if (op === 'ne') {
      return { bool: { must_not: obj } };
    } else {
      return { bool: { should: obj } };
    }
  }

  switch (info.type2 || info.type) {
  case 'ip':
    if (value[0] === '/') { throw value + ' - Regex not supported for ip queries'; }

    if (value.indexOf('*') !== -1) { throw value + ' - Wildcard not supported for ip queries'; }

    if (value === 'ipv4') {
      value = '0.0.0.0/0';
    } if (value === 'ipv6') {
      value = '0.0.0.0/0';
      if (op === 'ne') { op = 'eq'; } else { op = 'ne'; }
    }

    if (op === 'eq') {
      return parseIpPort(yy, field, value);
    }
    if (op === 'ne') {
      return { bool: { must_not: parseIpPort(yy, field, value) } };
    }

    if (isArrayStart(value)) { throw value + ' - List queries not supported for gt/lt queries - ' + value; }

    obj = { range: {} };
    obj.range[info.dbField] = {};
    obj.range[info.dbField][op] = value;
    return obj;
  case 'integer':
    if (value[0] === '/') { throw value + ' - Regex queries not supported for integer queries'; }

    if (op === 'eq' || op === 'ne') {
      obj = termOrTermsInt(info.dbField, value);
      if (op === 'ne') {
        obj = { bool: { must_not: obj } };
      }
      return obj;
    }

    if (isArrayStart(value)) { throw value + ' - List queries not supported for gt/lt queries - ' + value; }

    obj = { range: {} };
    obj.range[info.dbField] = {};
    obj.range[info.dbField][op] = parseInt(value);
    return obj;
  case 'float':
    if (value[0] === '/') { throw value + ' - Regex queries not supported for float queries'; }

    if (op === 'eq' || op === 'ne') {
      obj = termOrTermsFloat(info.dbField, value);
      if (op === 'ne') {
        obj = { bool: { must_not: obj } };
      }
      return obj;
    }

    if (isArrayStart(value)) { throw value + ' - List queries not supported for gt/lt queries - ' + value; }

    obj = { range: {} };
    obj.range[info.dbField] = {};
    obj.range[info.dbField][op] = parseFloat(value);
    return obj;
  case 'lotermfield':
  case 'lotextfield':
    if (op === 'eq') { return stringQuery(yy, field, value.toLowerCase()); }
    if (op === 'ne') { return { bool: { must_not: stringQuery(yy, field, value.toLowerCase()) } }; }
    throw "Invalid operator '" + op + "' for " + field;
  case 'termfield':
  case 'textfield':
    if (op === 'eq') { return stringQuery(yy, field, value); }
    if (op === 'ne') { return { bool: { must_not: stringQuery(yy, field, value) } }; }
    throw "Invalid operator '" + op + "' for " + field;
  case 'uptermfield':
  case 'uptextfield':
    if (op === 'eq') { return stringQuery(yy, field, value.toUpperCase()); }
    if (op === 'ne') { return { bool: { must_not: stringQuery(yy, field, value.toUpperCase()) } }; }
    throw "Invalid operator '" + op + "' for " + field;
  case 'fileand':
    if (isArrayStart(value)) { throw value + ' - List queries not supported for file queries - ' + value; }

    if (op === 'eq') { return { fileand: stripQuotes(value) }; }
    if (op === 'ne') { return { bool: { must_not: { fileand: stripQuotes(value) } } }; }
    throw op + ' - not supported for file queries';
  case 'viewand':
    if (isArrayStart(value)) { throw value + ' - List queries not supported for view queries - ' + value; }

    value = stripQuotes(value);
    if (!yy.views || !yy.views[value]) { throw value + ' - View not found for user'; }

    if (op === 'eq') { return exports.parse(yy.views[value].expression); }
    if (op === 'ne') { return { bool: { must_not: exports.parse(yy.views[value].expression) } }; }
    throw op + ' - not supported for view queries';
  case 'seconds':
    if (value[0] === '/') { throw value + ' - Regex queries not supported for date queries'; }

    if (op === 'eq' || op === 'ne') {
      obj = termOrTermsSeconds(info.dbField, value);
      if (op === 'ne') {
        obj = { bool: { must_not: obj } };
      }
      return obj;
    }

    if (isArrayStart(value)) { throw value + ' - List queries not supported for gt/lt queries - ' + value; }

    obj = { range: {} };
    obj.range[info.dbField] = {};
    obj.range[info.dbField][op] = parseSeconds(stripQuotes(value));
    return obj;
  case 'date':
    if (value[0] === '/') { throw value + ' - Regex queries not supported for date queries'; }

    if (op === 'eq' || op === 'ne') {
      obj = termOrTermsDate(info.dbField, value);
      if (op === 'ne') {
        obj = { bool: { must_not: obj } };
      }
      return obj;
    }

    if (isArrayStart(value)) { throw value + ' - List queries not supported for gt/lt queries - ' + value; }

    obj = { range: {} };
    obj.range[info.dbField] = {};
    obj.range[info.dbField][op] = moment.unix(parseSeconds(stripQuotes(value))).format();
    return obj;
  default:
    throw 'Unknown field type: ' + info.type;
  }
}

function checkRegex (str) {
  let m;
  if ((m = str.match(/^\/(?:\\?.)*?\//)) && m[0].length !== str.length) {
    throw 'Must back slash any forward slashes in regexp query - ' + m[0];
  }
}

function field2Raw (yy, field) {
  const info = getFieldInfo(yy, field);
  const dbField = info.dbField;
  if (info.rawField) { return info.rawField; }

  if (dbField.indexOf('.snow', dbField.length - 5) === 0) { return dbField.substring(0, dbField.length - 5) + '.raw'; }

  return dbField;
}

function stringQuery (yy, field, str) {
  const info = getFieldInfo(yy, field);
  let dbField = info.dbField;
  let obj;

  if (str[0] === '/' && str[str.length - 1] === '/') {
    checkRegex(str);

    str = str.substring(1, str.length - 1);
    if (info.transform) {
      str = global.moloch[info.transform](str).replace(/2e/g, '.');
    }
    dbField = field2Raw(yy, field);
    obj = { regexp: {} };
    obj.regexp[dbField] = str.replace(/\\(.)/g, '$1');
    return obj;
  }

  let quoted = false;
  if (str[0] === '"' && str[str.length - 1] === '"') {
    str = str.substring(1, str.length - 1).replace(/\\(.)/g, '$1');
    quoted = true;
  } else if (isArrayFull(str)) {
    const rawField = field2Raw(yy, field);
    const strs = ListToArrayMap(str, global.moloch[info.transform]);

    const items = [];
    let terms = null;
    strs.forEach(function (astr) {
      let item;

      if (typeof astr === 'string' && astr[0] === '/' && astr[astr.length - 1] === '/') {
        checkRegex(astr);

        item = { regexp: {} };
        item.regexp[rawField] = astr.substring(1, astr.length - 1);
        items.push(item);
      } else if (typeof astr === 'string' && astr.indexOf('*') !== -1) {
        if (astr === '*') {
          throw "Please use 'EXISTS!' instead of a '*' in expression";
        }

        item = { wildcard: {} };
        item.wildcard[rawField] = astr;
        items.push(item);
      } else {
        if (astr[0] === '"' && astr[astr.length - 1] === '"') {
          astr = astr.substring(1, astr.length - 1).replace(/\\(.)/g, '$1');
        }

        if (info.type.match(/termfield/)) {
          if (isArrayAND(str)) {
            item = { term: {} };
            item.term[dbField] = astr;
            items.push(item);
            return;
          }

          // Reuse same terms element for all terms query and add to items once
          if (terms === null) {
            terms = { terms: {} };
            terms.terms[dbField] = [];
            items.push(terms);
          }
          terms.terms[dbField].push(astr);
        } else {
          item = { match_phrase: {} };
          item.match_phrase[dbField] = astr;
          items.push(item);
        }
      }
    }); // forEach

    if (items.length === 1) {
      obj = items[0];
    } else if (isArrayAND(str)) {
      obj = { bool: { filter: items } };
    } else {
      obj = { bool: { should: items } };
    }

    return obj;
  }

  if (info.transform) {
    str = global.moloch[info.transform](str);
  }

  if (!isNaN(str) && !quoted) {
    obj = { term: {} };
    obj.term[dbField] = str;
  } else if (typeof str === 'string' && str.indexOf('*') !== -1) {
    if (str === '*') {
      throw "Please use 'EXISTS!' instead of a '*' in expression";
    }
    dbField = field2Raw(yy, field);
    obj = { wildcard: {} };
    obj.wildcard[dbField] = str;
  } else if (info.type.match(/textfield/)) {
    obj = { match_phrase: {} };
    obj.match_phrase[dbField] = str;
  } else if (info.type.match(/termfield/)) {
    obj = { term: {} };
    obj.term[dbField] = str;
  }

  return obj;
}

if (!global.moloch) global.moloch = {};
global.moloch.utf8ToHex = function (utf8) {
  let hex = Buffer.from(stripQuotes(utf8)).toString('hex').toLowerCase();
  hex = hex.replace(/2a/g, '*');
  return hex;
};

global.moloch.dash2Colon = function (str) {
  return str.replace(/-/g, ':');
};

const protocols = {
  icmp: 1,
  igmp: 2,
  tcp: 6,
  udp: 17,
  gre: 47,
  esp: 50,
  icmp6: 58,
  icmpv6: 58,
  ospf: 89,
  pim: 103,
  sctp: 132
};

global.moloch.ipProtocolLookup = function (text) {
  if (typeof text !== 'string') {
    for (let i = 0; i < text.length; i++) {
      if (!protocols[text[i]] && isNaN(text[i])) { throw ('Unknown protocol string ' + text); }
      text[i] = protocols[text[i]] || +text[i];
    }
    return text;
  } else {
    if (!protocols[text] && isNaN(text)) { throw ('Unknown protocol string ' + text); }
    return protocols[text] || +text;
  }
};

// Remove the "http://", "https://", etc from http.uri queries
global.moloch.removeProtocol = function (text) {
  if (text[0] === '/' && text[text.length - 1] === '/') {
    return text;
  }
  text = text.replace(/^[a-z]+:\/\//i, '');
  return text;
};

// Remove the "http://", "https://" and after the first slash, etc from host queries
global.moloch.removeProtocolAndURI = function (text) {
  if (text[0] === '/' && text[text.length - 1] === '/') {
    return text;
  }
  text = text.replace(/^[a-z]+:\/\//i, '');
  text = text.replace(/\/.*/, '');
  return text;
};

function isArrayAND(value) {
  return (value[0] === ']');
}

function isArrayStart(value) {
  if (value[0] === '[' || value[0] === ']') {
    return true;
  }
  return false;
}

function isArrayFull(value) {
  if (value[0] === '[' && value[value.length - 1] === ']') {
    return true;
  }
  if (value[0] === ']' && value[value.length - 1] === '[') {
    return true;
  }
  return false;
}


function ListToArray (text, always) {
  if (isArrayFull(text)) {
    text = text.substring(1, text.length - 1);
  } else if (!always) {
    return text;
  }

  // JS doesn't have negative look behind
  const strs = text.replace(/\\\\/g, '**BACKSLASH**').replace(/\\,/g, '**COMMA**').split(/\s*,\s*/).filter(part => part.trim()[0] !== '$');
  for (let i = 0; i < strs.length; i++) {
    strs[i] = strs[i].replace('**COMMA**', ',').replace('**BACKSLASH**', '\\');
  }
  return strs;
}

function ListToArrayMap (text, mapCb, always) {
  let list = ListToArray(text, always);

  if (!mapCb || !Array.isArray(list)) { return list; }

  return list.map(mapCb);
}

function ListToArrayShortcuts (yy, text) {
  if (isArrayFull(text)) {
    text = text.substring(1, text.length - 1);
  }

  // JS doesn't have negative look behind
  const strs = text.replace(/\\\\/g, '**BACKSLASH**').replace(/\\,/g, '**COMMA**').split(/\s*,\s*/).filter(part => part.trim()[0] === '$');
  const nstrs = [];
  for (let i = 0; i < strs.length; i++) {
    const str = strs[i].replace('**COMMA**', ',').replace('**BACKSLASH**', '\\');

    if (str.match(/[*?]/)) {
      const re = new RegExp('^' + str.substring(1).replace(/\*/g, '.*').replace(/\?/g, '.') + '$');
      Object.keys(yy.shortcuts).forEach((s) => {
        if (s.match(re)) { nstrs.push('$' + s); }
      });
    } else {
      nstrs.push(str);
    }
  }

  return nstrs;
}

function termOrTermsInt (dbField, str) {
  let obj = {};
  if (isArrayFull(str)) {
    if (isArrayAND(str)) {
      obj.bool = {
        filter: ListToArray(str).map(x => {
          const o = { term: {} };
          o.term[dbField] = parseInt(x);
          return o;
        })
      };
      return obj;
    }

    obj = { terms: {} };
    obj.terms[dbField] = ListToArray(str).map(x => parseInt(x));
  } else {
    str = stripQuotes(str);
    let match;
    if ((match = str.match(/(-?\d+)-(-?\d+)/))) {
      obj = { range: {} };
      obj.range[dbField] = { gte: parseInt(match[1]), lte: parseInt(match[2]) };
      return obj;
    } else if (str.match(/[^\d]+/)) {
      throw str + ' is not a number';
    }
    obj = { term: {} };
    obj.term[dbField] = parseInt(str);
  }
  return obj;
}

function termOrTermsFloat (dbField, str) {
  let obj = {};
  if (isArrayFull(str)) {
    if (isArrayAND(str)) {
      obj.bool = {
        filter: ListToArray(str).map(x => {
          const o = { term: {} };
          o.term[dbField] = parseFloat(x);
          return o;
        })
      };
      return obj;
    }

    obj = { terms: {} };
    obj.terms[dbField] = ListToArray(str).map(x => parseFloat(x));
  } else {
    str = stripQuotes(str);
    let match;
    if (str.match(/^-?\d+$/) || str.match(/^-?\d+\.\d+$/)) {
      // good non range
    } else if ((match = str.match(/(-?\d*\.?\d*)-(-?\d*\.?\d*)/))) {
      obj = { range: {} };
      obj.range[dbField] = { gte: parseFloat(match[1]), lte: parseFloat(match[2]) };
      return obj;
    } else if (!(str.match(/^-?\d+$/) || str.match(/^-?\d+\.\d+$/))) {
      throw str + ' is not a float';
    }
    obj = { term: {} };
    obj.term[dbField] = parseFloat(str);
  }
  return obj;
}

function termOrTermsSeconds (dbField, str) {
  let obj = {};
  if (isArrayFull(str)) {
    if (isArrayAND(str)) {
      obj.bool = {
        filter: ListToArray(str).map(x => {
          const o = { term: {} };
          o.term[dbField] = parseSeconds(x);
          return o;
        })
      };
      return obj;
    }

    obj = { terms: {} };
    obj.terms[dbField] = ListToArray(str).map(x => parseSeconds(x));
  } else {
    str = parseSeconds(stripQuotes(str));
    obj = { term: {} };
    obj.term[dbField] = str;
  }
  return obj;
}

// This uses weird gte/lte range of the same date because if you give a second
// date, you want everything that happen from 0ms-1000ms, not just at 0ms
function termOrTermsDate (dbField, str) {
  if (isArrayFull(str)) {
    const items = []
    ListToArray(str).forEach(function (astr) {
      const d = moment.unix(parseSeconds(stripQuotes(astr))).format();
      const r = { range: {} };
      r.range[dbField] = { gte: d, lte: d };
      items.push(r);
    });


    if (isArrayAND(str)) {
      return { bool: { filter: items } };
    } else {
      return { bool: { should: items } };
    }
  }

  const d = moment.unix(parseSeconds(stripQuotes(str))).format();
  const obj = { range: {} };
  obj.range[dbField] = { gte: d, lte: d };
  return obj;
}

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

function parseSeconds (str) {
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

  return new moment(str, ['YYYY/MM/DD HH:mm:ss', 'YYYY/MM/DD HH:mm:ss Z', moment.ISO_8601]).unix();
}
