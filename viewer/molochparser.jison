/* lexical grammar */
%lex

%options flex
%%

\s+                   /* skip whitespace */
[0-9]+\b                  return 'NUMBER'
([0-9]{1,3})?("."[0-9]{1,3})?("."[0-9]{1,3})?("."[0-9]{1,3})?("/"[0-9]{1,2})?(":"[0-9]{1,5})?\b return 'IPMATCH'
"bytes"                   return 'bytes'
"databytes"               return 'databytes'
"packets"                 return 'packets'
"protocol"                return 'protocol'
"port.src"                return 'port.src'
"port.dst"                return 'port.dst'
"port"                    return 'port'
"node"                    return 'node'
"country.src"             return 'country.src'
"country.dst"             return 'country.dst'
"country.xff"             return 'country.xff'
"country"                 return 'country'
"ip.src"                  return "ip.src"
"ip.dst"                  return "ip.dst"
"ip.xff"                  return "ip.xff"
"ip"                      return "ip"
"uri"                     return "uri"
"ua"                      return "ua"
"tcp"                     return "tcp"
"udp"                     return "udp"
"host"                    return "host"
"header"                  return "header"
"contains"                return 'contains'
"tags"                    return 'tags'
[\w*._:-]+                return 'ID'
\"[^"]+\"                 return 'QUOTEDSTR'
"<="                      return 'lte'
"<"                       return 'lt'
">="                      return 'gte'
">"                       return 'gt'
"!="                      return '!='
"=="                      return '=='
"="                       return '=='
"||"                      return '||'
"|"                       return '||'
"&&"                      return '&&'
"&"                       return '&&'
"("                       return '('
")"                       return ')'
"!"                       return '!'
<<EOF>>                   return 'EOF'
.                         return 'INVALID'

/lex

/* operator associations and precedence */

%left '!'
%left '<' '<=' '>' '>=' '==' '!=' contains
%left '||'
%left '&&'
%left UMINUS

%start expressions

%% /* language grammar */

expressions
    : e EOF
        { return $1; }
    ;

LTA : lt  {$$ = 'lt'}
    | lte {$$ = 'lte'}
    ;

GTA : gt  {$$ = 'gt'}
    | gte {$$ = 'gte'}
    ;

GTLT: LTA
    | GTA
    ;

IPNUM: IPMATCH
     | NUMBER
     ;

RANGEFIELD: databytes   {$$ = 'db'}
          | bytes       {$$ = 'by'}
          | packets     {$$ = 'pa'}
          | protocol    {$$ = 'pr'}
          | 'port.src'  {$$ = 'p1'}
          | 'port.dst'  {$$ = 'p2'}
          ;

STRFIELD  : 'country.src' {$$ = 'g1'}
          | 'country.dst' {$$ = 'g2'}
          | 'country.xff' {$$ = 'gxff'}
          | node          {$$ = 'no'}
          | host          {$$ = 'ho'}
          ;

STR : ID
    | packets
    | bytes
    | protocol
    | port
    | port.src
    | port.dst
    | country
    | country.src
    | country.dst
    | country.xff
    | QUOTEDSTR
    | node
    | host
    | header
    | tcp
    | contains
    | udp
    | ip
    | ip.src
    | ip.dst
    | ip.xff
    | uri
    | ua
    ;
 
e
    : e '&&' e
        {$$ = {and: [$1, $3]};}
    | 'uri' contains STR
        {$$ = {query: {text: {us: {query: $3, type: "phrase", operator: "and"}}}};}
    | 'ua' contains STR
        {$$ = {query: {text: {ua: {query: $3, type: "phrase", operator: "and"}}}};}
    | e '||' e
        {$$ = {or: [$1, $3]};}
    | '!' e %prec UMINUS
        {$$ = {not: $2};}
    | '-' e %prec UMINUS
        {$$ = -$2;}
    | '(' e ')'
        {$$ = $2;}
    | protocol '==' 'tcp'
        {$$ = {term: {pr: 6}};}
    | protocol '==' 'udp'
        {$$ = {term: {pr: 17}};}
    | RANGEFIELD GTLT NUMBER
        {$$ = {range: {}};
         $$.range[$1] = {};
         $$.range[$1][$2] = $3;}
    | RANGEFIELD '==' NUMBER
        {$$ = {term: {}};
         $$.term[$1] = $3;}
    | RANGEFIELD '!=' NUMBER
        {$$ = {not: {term: {}}};
         $$.not.term[$1] = $3;}
    | 'port' GTLT NUMBER
        {$$ = {or: [{range: {p1: {}}}, {range: {p2: {}}}]};
         $$.or[0].range.p1[$2] = $3;
         $$.or[1].range.p2[$2] = $3;}
    | STRFIELD '==' STR
        {$$ = {term: {}};
         $$.term[$1] = $3;}
    | STRFIELD '!=' STR
        {$$ = {not: {term: {}}};
         $$.not.term[$1] = $3;}
    | STRFIELD 'contains' STR
        { var str = stripQuotes($3);
          if (str.indexOf("*") !== -1) {
            $$ = {query: {wildcard: {}}};
            $$.query.wildcard[$1] = str;
          } else {
            $$ = {term: {}};
            $$.term[$1] = str;
          }
        }
    | 'port' '==' NUMBER
        {$$ = {or: [{term: {p1: $3}}, {term: {p2: $3}}]};}
    | 'port' '!=' NUMBER
        {$$ = {not: {or: [{term: {p1: $3}}, {term: {p2: $3}}]}};}
    | 'ip' '==' IPNUM
        {$$ = parseIpPort($3,0);}
    | 'ip' '!=' IPNUM
        {$$ = {not: parseIpPort($3,0)};}
    | 'ip.src' '==' IPNUM
        {$$ = parseIpPort($3,1);}
    | 'ip.src' '!=' IPNUM
        {$$ = {not: parseIpPort($3,1)};}
    | 'ip.dst' '==' IPNUM
        {$$ = parseIpPort($3,2);}
    | 'ip.dst' '!=' IPNUM
        {$$ = {not: parseIpPort($3,2)};}
    | 'ip.xff' '==' IPNUM
        {$$ = parseIpPort($3,3);}
    | 'ip.xff' '!=' IPNUM
        {$$ = {not: parseIpPort($3,3)};}
    | tags '==' STR
        { var tag = stripQuotes($3);
          $$ = {term: {ta: tag}};
        }
    | tags '!=' STR
        { var tag = stripQuotes($3);
          $$ = {not: {term: {ta: tag}}};
        }
    | tags contains STR
        { var tag = stripQuotes($3);
          $$ = {term: {ta: tag}};
        }
    | header '==' STR
        { var tag = stripQuotes($3);
          $$ = {term: {hh: tag}};
        }
    | header '!=' STR
        { var tag = stripQuotes($3);
          $$ = {not: {term: {hh: tag}}};
        }
    | header contains STR
        { var tag = stripQuotes($3);
          $$ = {term: {hh: tag}};
        }
    | country '==' ID {$$ = {or: [{term: {g1: $3.toUpperCase()}}, {term: {g2: $3.toUpperCase()}}, {term: {gxff: $3.toUpperCase()}}]};}
    | country '!=' ID {$$ = {not: {or: [{term: {g1: $3.toUpperCase()}}, {term: {g2: $3.toUpperCase()}}, {term: {gxff: $3.toUpperCase()}}]}};}
    | country 'contains' ID {$$ = {or: [{query: {wildcard: {g1: $3.toUpperCase()}}}, {query: {wildcard: {g2: $3.toUpperCase()}}}, {query: {wildcard: {gxff: $3.toUpperCase()}}}]};}
    | STRFIELD 'contains' STR
        { var str = stripQuotes($3);
          if (str.indexOf("*") !== -1) {
            $$ = {query: {wildcard: {}}};
            $$.query.wildcard[$1] = str;
          } else {
            $$ = {term: {}};
            $$.term[$1] = str;
          }
        }
    ;
%%
function parseIpPort(ipPortStr, which) {
  ipPortStr = ipPortStr.trim();
  // Support '10.10.10/16:4321'

  var ip1 = -1, ip2 = -1;
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
     var s = parseInt(slash[1], 10);
     ip1 = ip1 & (0xffffffff << (32 - s));
     ip2 = ip2 | (0xffffffff >>> s);
  }

  var t1 = {and: []};
  var t2 = {and: []};
  var xff;

  if (ip1 !== -1) {
    if (ip1 === ip2) {
        t1  = {term: {a1: ip1>>>0}};
        t2  = {term: {a2: ip1>>>0}};
        xff = {term: {xff: ip1>>>0}};
    } else {
        t1.and.push({range: {a1: {from: ip1>>>0, to: ip2>>>0}}});
        t2.and.push({range: {a2: {from: ip1>>>0, to: ip2>>>0}}});
        xff =  {range: {xff: {from: ip1>>>0, to: ip2>>>0}}};
    }
  }

  if (port !== -1) {
    t1.and.push({term: {p1: port}});
    t2.and.push({term: {p2: port}});
  }

  switch(which) {
  case 0:
    return {or: [t1, t2, xff]};
  case 1:
    return t1;
  case 2:
    return t2;
  case 3:
    return xff;
  }
}

function stripQuotes (str) {
  if (str[0] === "\"") {
    str =  str.substring(1, str.length-1);
  }
  return str;
}
