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
"port"                    return 'port'
"port1"                   return 'port1'
"port2"                   return 'port2'
"node"                    return 'node'
"country"                 return 'country'
"country1"                return 'country1'
"country2"                return 'country2'
"ip"                      return "ip"
"ip1"                     return "ip1"
"ip2"                     return "ip2"
"uri"                     return "uri"
"tcp"                     return "tcp"
"udp"                     return "udp"
"host"                    return "host"
"contains"                return 'contains'
"tags"                    return 'tags'
[\w*._-]+                 return 'ID'
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

RANGEFIELD: databytes {$$ = 'db'}
          | bytes     {$$ = 'by'}
          | packets   {$$ = 'pa'}
          | protocol  {$$ = 'pr'}
          | port1     {$$ = 'p1'}
          | port2     {$$ = 'p2'}
          ;

STRFIELD  : country1 {$$ = 'g1'}
          | country2 {$$ = 'g2'}
          | node     {$$ = 'no'}
          | host     {$$ = 'ho'}
          ;

STR : ID
    | packets
    | bytes
    | protocol
    | port
    | port1
    | port2
    | country
    | country1
    | country2
    | QUOTEDSTR
    | node
    | host
    | tcp
    | contains
    | udp
    | ip
    | ip1
    | ip2
    | uri
    ;
 
e
    : e '&&' e
        {$$ = {and: [$1, $3]};}
    | 'uri' contains STR
        {$$ = {query: {text: {us: {query: $3, type: "phrase", operator: "and"}}}};}
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
        {$$ = {numeric_range: {}};
         $$.numeric_range[$1] = {};
         $$.numeric_range[$1][$2] = $3;}
    | RANGEFIELD '==' NUMBER
        {$$ = {term: {}};
         $$.term[$1] = $3;}
    | RANGEFIELD '!=' NUMBER
        {$$ = {not: {term: {}}};
         $$.not.term[$1] = $3;}
    | 'port' GTLT NUMBER
        {$$ = {or: [{numeric_range: {p1: {}}}, {numeric_range: {p2: {}}}]};
         $$.or[0].numeric_range.p1[$2] = $3;
         $$.or[1].numeric_range.p2[$2] = $3;}
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
    | 'ip1' '==' IPNUM
        {$$ = parseIpPort($3,1);}
    | 'ip1' '!=' IPNUM
        {$$ = {not: parseIpPort($3,1)};}
    | 'ip2' '==' IPNUM
        {$$ = parseIpPort($3,2);}
    | 'ip2' '!=' IPNUM
        {$$ = {not: parseIpPort($3,2)};}
    | tags contains STR
        { var tag = stripQuotes($3);
          $$ = {term: {ta: tag}};
        }
    | country '==' ID {$$ = {or: [{term: {g1: $3.toUpperCase()}}, {term: {g2: $3.toUpperCase()}}]};}
    | country '!=' ID {$$ = {not: {or: [{term: {g1: $3.toUpperCase()}}, {term: {g2: $3.toUpperCase()}}]}};}
    | country 'contains' ID {$$ = {or: [{query: {wildcard: {g1: $3.toUpperCase()}}}, {query: {wildcard: {g2: $3.toUpperCase()}}}]};}
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
  console.log("Looking at ", ipPortStr);
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

  if (ip1 !== -1) {
    t1.and.push({numeric_range: {a1: {from: ip1>>>0, to: ip2>>>0}}});
    t2.and.push({numeric_range: {a2: {from: ip1>>>0, to: ip2>>>0}}});
  }

  if (port !== -1) {
    t1.and.push({term: {p1: port}});
    t2.and.push({term: {p2: port}});
  }

  if (which === 0) {
    return {or: [t1, t2]};
  } 
  if (which === 1) {
    return t1;
  }
  if (which === 2) {
    return t2;
  }

}
function stripQuotes (str) {
  if (str[0] === "\"") {
    str =  str.substring(1, str.length-1);
  }
  return str;
}
