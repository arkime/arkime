/* lexical grammar */
%lex

%options flex
%%

\s+                        /* skip whitespace */
\"(?:\\?.)*?\"             return 'QUOTEDSTR'
\/(?:\\?.)*?\/             return 'REGEXSTR'
[-+a-zA-Z0-9_.@:*?/]+      return 'STR'
\[[^\]\\]*(?:\\.[^\]\\]*)*\] return 'LIST'
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
        {$$ = {bool: {must: [$1, $3]}};}
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

var    util           = require('util');
var    moment         = require('moment');

function parseIpPort(yy, field, ipPortStr) {
  var dbField = yy.fieldsMap[field].dbField;

  function singleIp(dbField, ip1, ip2, port) {
    var obj;

    if (ip1 !== undefined) {
      if (ip1 === ip2) {
        obj = {term: {}};
        obj.term[dbField] = ip1>>>0;
      } else {
        obj = {range: {}};
        obj.range[dbField] = {from: ip1>>>0, to: ip2>>>0};
      }
    }

    if (port !== -1) {
      if (yy.fieldsMap[field].portField) {
        obj = {bool: {must: [obj, {term: {}}]}};
        obj.bool.must[1].term[yy.fieldsMap[field].portField] = port;
      } else {
        throw field + " doesn't support port";
      }

      if (ip1 === undefined) {
        obj = obj.bool.must[1];
      }
    }

    return obj;
  }


  var obj;

  ipPortStr = ipPortStr.trim();

// We really have a list of them
  if (ipPortStr[0] === "[" && ipPortStr[ipPortStr.length -1] === "]") {
      obj =  {bool: {should: []}};
      ListToArray(ipPortStr).forEach(function(str) {
        obj.bool.should.push(parseIpPort(yy, field, str));
      });
      // Optimize 1 item in array
      if (obj.bool.should.length === 1) {
        obj = obj.bool.should[0];
      }
      return obj;
  }

  // Support '10.10.10/16:4321'

  var ip1, ip2;
  var colons = ipPortStr.split(':');
  var slash = colons[0].split('/');
  var dots = slash[0].split('.');
  var port = -1;
  if (colons[1]) {
    port = parseInt(colons[1], 10);
  }

  if (dots.length === 4) {
    ip1 = ip2 = (parseInt(dots[0], 10) << 24) | (parseInt(dots[1], 10) << 16) | (parseInt(dots[2], 10) << 8) | parseInt(dots[3], 10);
  } else if (dots.length === 3) {
    ip1 = (parseInt(dots[0], 10) << 24) | (parseInt(dots[1], 10) << 16) | (parseInt(dots[2], 10) << 8);
    ip2 = (parseInt(dots[0], 10) << 24) | (parseInt(dots[1], 10) << 16) | (parseInt(dots[2], 10) << 8) | 255;
  } else if (dots.length === 2) {
    ip1 = (parseInt(dots[0], 10) << 24) | (parseInt(dots[1], 10) << 16);
    ip2 = (parseInt(dots[0], 10) << 24) | (parseInt(dots[1], 10) << 16) | (255 << 8) | 255;
  } else if (dots.length === 1 && dots[0].length > 0) {
    ip1 = (parseInt(dots[0], 10) << 24);
    ip2 = (parseInt(dots[0], 10) << 24) | (255 << 16) | (255 << 8) | 255;
  }

  // Can't shift by 32 bits in javascript, who knew!
  if (slash[1] && slash[1] !== '32') {
    if (ip1 === undefined) {
      ip1 = ip2 = 0xffffffff;
    }
    var s = parseInt(slash[1], 10);
    ip1 = ip1 & (0xffffffff << (32 - s));
    ip2 = ip2 | (0xffffffff >>> s);
  }
  
  if (dbField !== "ipall") {
    return singleIp(dbField, ip1, ip2, port);
  }

  var ors = [];
  var completed = {};
  for (field in yy.fieldsMap) {
    var info = yy.fieldsMap[field];

    // If ip itself or not an ip field stop
    if (field === "ip" || info.type !== "ip")
      continue;

    // Already completed
    if (completed[info.dbField])
      continue;
    completed[info.dbField] = 1;

    // If port specified then skip ips without ports
    if (port !== -1 && !info.portField)
      continue;

    if (info.requiredRight && yy[info.requiredRight] !== true) {
      continue;
    }
    obj = singleIp(info.dbField, ip1, ip2, port);
    if (obj) {
      ors.push(obj);
    }
  }

  return {bool: {should: ors}};
}

function stripQuotes (str) {
  if (str[0] === "\"") {
    str =  str.substring(1, str.length-1);
  }
  return str;
}

function formatExists(yy, field, op)
{
  if (!yy.fieldsMap[field])
    throw "Unknown field " + field;

  var info = yy.fieldsMap[field];

  if (info.requiredRight && yy[info.requiredRight] !== true) {
    throw field + " - permission denied";
  }

  if (info.regex) {
    var regex = new RegExp(info.regex);
    var obj = [];
    var completed = [];
    for (var f in yy.fieldsMap) {
      if (f.match(regex) && !completed[yy.fieldsMap[f].dbField]) {
        if (yy.fieldsMap[f].requiredRight && yy[yy.fieldsMap[f].requiredRight] !== true) {
          continue;
        }
        obj.push(formatExists(yy, f, "eq"));
        completed[yy.fieldsMap[f].dbField] = 1;
      }
    }
    if (op === "ne") {
      return {bool: {must_not: obj}};
    } else {
      return {bool: {should: obj}};
    }
  }

  if (op === "ne") {
    return {bool: {must_not: {exists: {field: field2Raw(yy, field)}}}};
  }

  return {exists: {field: field2Raw(yy, field)}};
}

function formatQuery(yy, field, op, value)
{
  var obj;
  //console.log("field", field, "op", op, "value", value);
  //console.log("yy", util.inspect(yy, false, 50));
  if (value[0] === "/" && value[value.length -1] === "/") {
    checkRegex(value);
  }

  if (!yy.fieldsMap[field])
    throw "Unknown field " + field;

  var info = yy.fieldsMap[field];

  if (info.requiredRight && yy[info.requiredRight] !== true) {
    throw field + " - permission denied";
  }

  if (info.regex) {
    var regex = new RegExp(info.regex);
    var obj = [];
    var completed = [];
    for (var f in yy.fieldsMap) {
      if (f.match(regex) && !completed[yy.fieldsMap[f].dbField]) {
        if (yy.fieldsMap[f].requiredRight && yy[yy.fieldsMap[f].requiredRight] !== true) {
          continue;
        }
        /* If a not equal op then format as if an equal and do the not below */
        obj.push(formatQuery(yy, f, (op === "ne"?"eq":op), value));
        completed[yy.fieldsMap[f].dbField] = 1;
      }
    }
    if (op === "ne")
      return {bool: {must_not: obj}};
    else
      return {bool: {should: obj}};
    throw "Invalid operator '" + op + "' for " + field;
  }

  switch (info.type) {
  case "ip":
    if (value[0] === "/")
      throw value + " - Regex not supported for ip queries";

    if (value.indexOf("*") !== -1)
      throw value + " - Wildcard not supported for ip queries";

    if (op === "eq")
      return parseIpPort(yy, field, value);
    if (op === "ne")
      return {bool: {must_not: parseIpPort(yy, field, value)}};
    throw "Invalid operator '" + op + "' for ip";
  case "integer":
    if (value[0] === "/")
      throw value + " - Regex queries not supported for integer queries";

    if (op === "eq" || op === "ne") {
      obj = termOrTermsInt(info.dbField, value);
      if (op === "ne") {
        obj = {bool: {must_not: obj}};
      }
      return obj;
    }

    if (value[0] === "\[")
      throw value + " - List queries not supported for gt/lt queries - " + value;

    obj = {range: {}};
    obj.range[info.dbField] = {};
    obj.range[info.dbField][op] = value;
    return obj;
  case "lotermfield":
  case "lotextfield":
    if (op === "eq")
      return stringQuery(yy, field, value.toLowerCase());
    if (op === "ne")
      return {bool: {must_not: stringQuery(yy, field, value.toLowerCase())}};
    throw "Invalid operator '" + op + "' for " + field;
  case "termfield":
  case "textfield":
    if (op === "eq")
      return stringQuery(yy, field, value);
    if (op === "ne")
      return {bool: {must_not: stringQuery(yy, field, value)}};
    throw "Invalid operator '" + op + "' for " + field;
  case "uptermfield":
  case "uptextfield":
    if (op === "eq")
      return stringQuery(yy, field, value.toUpperCase());
    if (op === "ne")
      return {bool: {must_not: stringQuery(yy, field, value.toUpperCase())}};
    throw "Invalid operator '" + op + "' for " + field;
  case "fileand":
    if (value[0] === "\[")
      throw value + " - List queries not supported for file queries - " + value;

    if (op === "eq")
      return {fileand: stripQuotes(value)}
    if (op === "ne")
      return {bool: {must_not: {findand: stripQuotes(value)}}};
    throw op + " - not supported for file queries";
    break;
  case "viewand":
    if (value[0] === "\[")
      throw value + " - List queries not supported for view queries - " + value;

    value = stripQuotes(value);
    if (!yy.views || !yy.views[value])
        throw value + " - View not found for user";

    if (op === "eq")
      return exports.parse(yy.views[value].expression);
    if (op === "ne")
      return {bool: {must_not: exports.parse(yy.views[value].expression)}};
    throw op + " - not supported for view queries";
    break;
  case "seconds":
    if (value[0] === "/")
      throw value + " - Regex queries not supported for date queries";

    if (op === "eq" || op === "ne") {
      obj = termOrTermsSeconds(info.dbField, value);
      if (op === "ne") {
        obj = {bool: {must_not: obj}};
      }
      return obj;
    }

    if (value[0] === "\[")
      throw value + " - List queries not supported for gt/lt queries - " + value;

    obj = {range: {}};
    obj.range[info.dbField] = {};
    obj.range[info.dbField][op] = parseSeconds(stripQuotes(value));
    return obj;
  default:
    throw "Unknown field type: " + info.type;
  }
}

function checkRegex(str) {
    var m;
    if ((m = str.match(/^\/(?:\\?.)*?\//)) && m[0].length != str.length) {
      throw "Must back slash any forward slashes in regexp query - " + m[0];
    }
}

function field2Raw(yy, field) {
  var info = yy.fieldsMap[field];
  var dbField = info.dbField;
  if (info.rawField)
    return info.rawField;

  if (dbField.indexOf(".snow", dbField.length - 5) === 0)
    return dbField.substring(0, dbField.length - 5) + ".raw";

  return dbField;
}

function stringQuery(yy, field, str) {

  var info = yy.fieldsMap[field];
  var dbField = info.dbField;


  if (str[0] === "/" && str[str.length -1] === "/") {
    checkRegex(str);

    str = str.substring(1, str.length-1);
    if (info.transform) {
      str = global.moloch[info.transform](str).replace(/2e/g, '.');
    }
    dbField = field2Raw(yy, field);
    obj = {regexp: {}};
    obj.regexp[dbField] = str.replace(/\\(.)/g, "$1");
    return obj;
  }


  var quoted = false;
  if (str[0] === "\"" && str[str.length -1] === "\"") {
    str = str.substring(1, str.length-1).replace(/\\(.)/g, "$1");
    quoted = true;
  } else if (str[0] === "[" && str[str.length -1] === "]") {
    var rawField = field2Raw(yy, field);
    strs = ListToArray(str);
    if (info.transform) {
      for (var i = 0; i < strs.length; i++) {
        strs[i] = global.moloch[info.transform](strs[i]);
      }
    }

    var obj =  [];
    var terms = null;
    strs.forEach(function(str) {
      var should;

      if (typeof str === "string" && str[0] === "/" && str[str.length -1] === "/") {
        checkRegex(str);

        should = {regexp: {}};
        should.regexp[rawField] = str.substring(1, str.length-1);
        obj.push(should);
      } else if (typeof str === "string" && str.indexOf("*") !== -1) {
        if (str === "*") {
          throw "Please use 'EXISTS!' instead of a '*' in expression";
        }

        should = {wildcard: {}};
        should.wildcard[rawField] = str;
        obj.push(should);
      } else {
        if (str[0] === "\"" && str[str.length -1] === "\"") {
          str = str.substring(1, str.length-1).replace(/\\(.)/g, "$1");
        }

        if (info.type.match(/termfield/)) {
          // Reuse same terms element
          if (terms === null) {
            terms = {terms: {}};
            terms.terms[dbField] = [];
            obj.push(terms);
          }
          terms.terms[dbField].push(str);
        } else {
          should = {match: {}};
          should.match[dbField] = {query: str, type: "phrase", operator: "and"}
          obj.push(should);
        }
      }
    });

    if (obj.length === 1) {
      obj = obj[0];
    } else {
      obj = {bool: {should: obj}};
    }

    return obj;
  }

  if (info.transform) {
    str = global.moloch[info.transform](str);
  }

  if (!isNaN(str) && !quoted) {
    obj = {term: {}};
    obj.term[dbField] = str;
  } else if (typeof str === "string" && str.indexOf("*") !== -1) {
    if (str === "*") {
      throw "Please use 'EXISTS!' instead of a '*' in expression";
    }
    dbField = field2Raw(yy, field);
    obj = {wildcard: {}};
    obj.wildcard[dbField] = str;
  } else if (info.type.match(/textfield/)) {
    obj = {match: {}};
    obj.match[dbField] = {query: str, type: "phrase", operator: "and"}
  } else if (info.type.match(/termfield/)) {
    obj = {term: {}};
    obj.term[dbField] = str;
  }

  return obj;
}

if (!global.moloch) global.moloch = {};
global.moloch.utf8ToHex = function (utf8) {
    var hex = new Buffer(stripQuotes(utf8)).toString("hex").toLowerCase();
    hex = hex.replace(/2a/g, '*');
    return hex;
}

global.moloch.ipv6ToHex = function (ip) {
  ip = stripQuotes(ip);
  if (ip[0] === "[") {
    var closing = ip.indexOf(']');
    ip =  ip.substring(1, closing);
  }

  if (ip.indexOf("*") !== -1 && ip.indexOf("::") !== -1) {
    throw "Can't use :: in ipv6 and * at the same time";
  }
  var parts = ip.split(":");
  for (var i = 0; i < parts.length; i++) {
    if (parts[i].indexOf("*") !== -1) {
      continue;
    }
    if (parts[i] === "") {
      for (var j = 0; j <= 8 - parts.length; j++) {
        parts[i] += "0000";
      }
    } else {
      while (parts[i].length < 4) {
        parts[i] = "0" + parts[i];
      }
    }
  }
  return parts.join("");
}

var protocols = {
    icmp:   1,
    tcp:    6,
    udp:    17,
    icmp6:  58,
    icmpv6: 58
};

global.moloch.ipProtocolLookup = function (text) {
    if (typeof text !== "string") {
        for (var i = 0; i < text.length; i++) {
            if (!protocols[text[i]] && isNaN(text[i]))
                throw ("Unknown protocol string " + text);
            text[i] = protocols[text[i]] || +text[i];
        }
        return text;
    } else {
        if (!protocols[text] && isNaN(text))
            throw ("Unknown protocol string " + text);
        return protocols[text] || +text;
    }
};

function ListToArray(text) {
  if (text[0] !== "[" || text[text.length -1] !== "]")
    return text;

  // JS doesn't have negative look behind
  text = text.substring(1, text.length-1);
  strs = text.replace(/\\\\/g, "**BACKSLASH**").replace(/\\,/g, "**COMMA**").split(/\s*,\s*/);
  for (var i = 0; i < strs.length; i++) {
    strs[i] = strs[i].replace("**COMMA**", ",").replace("**BACKSLASH**", "\\");
  }
  return strs;
}

function termOrTermsInt(dbField, str) {
  var obj = {};
  if (str[0] === "[" && str[str.length -1] === "]") {
    obj = {terms: {}};
    obj.terms[dbField] = ListToArray(str);
    obj.terms[dbField].forEach(function(str) {
      str = stripQuotes(str);
      if (typeof str !== "integer" && str.match(/[^\d]+/))
        throw str + " is not a number";
    });
  } else {
    str = stripQuotes(str);
    if (str.match(/[^\d]+/))
      throw str + " is not a number";
    obj = {term: {}};
    obj.term[dbField] = str;
  }
  return obj;
}

function termOrTermsSeconds(dbField, str) {
  var obj = {};
  if (str[0] === "[" && str[str.length -1] === "]") {
    obj = {terms: {}};
    obj.terms[dbField] = ListToArray(str);
    obj.terms[dbField].forEach(function(str) {
      str = parseSeconds(stripQuotes(str));
    });
  } else {
    str = parseSeconds(stripQuotes(str));
    obj = {term: {}};
    obj.term[dbField] = str;
  }
  return obj;
}

function str2format(str) {
  var m;
  if (str.match(/^(s|sec|secs|second|seconds)$/)) {
    return "seconds";
  } else if (str.match(/^(m|min|mins|minute|minutes)$/)) {
    return "minutes";
  } else if (str.match(/^(h|hr|hrs|hour|hours)$/)) {
    return "hours";
  } else if (str.match(/^(d|day|days)$/)) {
    return "days";
  } else if (m = str.match(/^(w|week|weeks)\d*$/)) {
    return "weeks";
  } else if (str.match(/^(M|mon|mons|month|months)$/)) {
    return "months";
  } else if (str.match(/^(q|qtr|qtrs|quarter|quarters)$/)) {
    return "quarters";
  } else if (str.match(/^(y|yr|yrs|year|years)$/)) {
    return "years";
  }
  return undefined;
}

function parseSeconds(str) {
  var m, n;
  if (m = str.match(/^([+-])(\d*)([a-z]*)([@]*)([a-z0-9]*)/)) {

    var d       = moment();
    var format  = str2format(m[3]);
    var snap    = str2format(m[5]);

    if (m[2] === "") {
      m[2] = 1;
    }

    if (snap) {
      d.startOf(snap); 
      if (n = m[5].match(/^(w|week|weeks)(\d+)$/)) {
        d.day(n[2]);
      }
    }


    d.add((m[1]==='-'?-1:1)* m[2], format);
    return d.unix();
  }

  if (m = str.match(/^@([a-z0-9]+)/)) {

    var d       = moment();
    var snap    = str2format(m[1]);

    d.startOf(snap);
    if (n = m[1].match(/^(w|week|weeks)(\d+)$/)) {
      d.day(n[2]);
    }
    return d.unix();
  }

  return new moment(str, ["YYYY/MM/DD HH:mm:ss", "YYYY/MM/DD HH:mm:ss Z", moment.ISO_8601]).unix();
}
