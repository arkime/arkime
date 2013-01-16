/* lexical grammar */
%lex

%options flex
%%

\s+                   /* skip whitespace */
[0-9]+\b                  return 'NUMBER'
([0-9]{1,3})?("."[0-9]{1,3})?("."[0-9]{1,3})?("."[0-9]{1,3})?("/"[0-9]{1,2})?(":"[0-9]{1,5})?\b return 'IPMATCH'
"asn"                     return 'asn'
"asn.dns"                 return 'asn.dns'
"asn.dst"                 return 'asn.dst'
"asn.src"                 return 'asn.src'
"asn.xff"                 return 'asn.xff'
"bytes"                   return 'bytes'
"cert.alt.cnt"            return "cert.alt.cnt"
"cert.alt"                return "cert.alt"
"cert.cnt"                return "cert.cnt"
"cert.issuer.cn"          return "cert.issuer.cn"
"cert.issuer.on"          return "cert.issuer.on"
"cert.serial"             return "cert.serial"
"cert.subject.cn"         return "cert.subject.cn"
"cert.subject.on"         return "cert.subject.on"
"country"                 return 'country'
"country.dns"             return 'country.dns'
"country.dst"             return 'country.dst'
"country.src"             return 'country.src'
"country.xff"             return 'country.xff'
"databytes"               return 'databytes'
"header.dst.cnt"          return "header.dst.cnt"
"header.dst"              return "header.dst"
"header"                  return "header"
"header.src.cnt"          return "header.src.cnt"
"header.src"              return "header.src"
"host.cnt"                return "host.cnt"
"host"                    return "host"
"icmp"                    return "icmp"
"id"                      return "id"
"ip.dns.cnt"              return "ip.dns.cnt"
"ip.dns"                  return "ip.dns"
"ip.dst"                  return "ip.dst"
"ip"                      return "ip"
"ip.src"                  return "ip.src"
"ip.xff.cnt"              return "ip.xff.cnt"
"ip.xff"                  return "ip.xff"
"node"                    return 'node'
"oldheader"               return "oldheader"
"packets"                 return 'packets'
"port.dst"                return 'port.dst'
"port"                    return 'port'
"port.src"                return 'port.src'
"protocol"                return 'protocol'
"ssh.key.cnt"             return "ssh.key.cnt"
"ssh.key"                 return "ssh.key"
"ssh.ver.cnt"             return "ssh.ver.cnt"
"ssh.ver"                 return "ssh.ver"
"tags.cnt"                return 'tags.cnt'
"tags"                    return 'tags'
"tcp"                     return "tcp"
"ua.cnt"                  return "ua.cnt"
"ua"                      return "ua"
"udp"                     return "udp"
"uri.cnt"                 return "uri.cnt"
"uri"                     return "uri"
"user.cnt"                return "user.cnt"
"user"                    return "user"
[/\w*._:-]+               return 'ID'
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

RANGEFIELD: databytes         {$$ = 'db'}
          | bytes             {$$ = 'by'}
          | packets           {$$ = 'pa'}
          | protocol          {$$ = 'pr'}
          | 'port.src'        {$$ = 'p1'}
          | 'port.dst'        {$$ = 'p2'}
          | 'uri.cnt'         {$$ = 'uscnt'}
          | 'cert.cnt'        {$$ = 'tlscnt'}
          | 'ip.xff.cnt'      {$$ = 'xffscnt'}
          | 'ip.dns.cnt'      {$$ = 'dnsipcnt'}
          | 'ua.cnt'          {$$ = 'uacnt'}
          | 'user.cnt'        {$$ = 'usercnt'}
          | 'host.cnt'        {$$ = 'hocnt'}
          | 'header.src.cnt'  {$$ = 'hh1cnt'}
          | 'header.dst.cnt'  {$$ = 'hh2cnt'}
          | 'tags.cnt'        {$$ = 'tacnt'}
          | 'cert.alt.cnt'    {$$ = 'tls.altcnt'}
          | 'ssh.key.cnt'     {$$ = 'sshkeycnt'}
          | 'ssh.ver.cnt'     {$$ = 'sshvercnt'}
          ;

LOTERMFIELD  : node              {$$ = 'no'}
             | host              {$$ = 'ho'}
             | user              {$$ = 'user'}
             | 'cert.subject.cn' {$$ = 'tls.sCn'}
             | 'cert.issuer.cn'  {$$ = 'tls.iCn'}
             | 'cert.serial'     {$$ = 'tls.sn'}
             | 'cert.alt'        {$$ = 'tls.alt'}
             | 'ssh.ver'         {$$ = 'sshver'}
             ;

TERMFIELD  : 'id' {$$ = '_id'}
           | 'ssh.key' {$$ = 'sshkey'}
           ;

UPTERMFIELD  : 'country.src' {$$ = 'g1'}
             | 'country.dst' {$$ = 'g2'}
             | 'country.dns' {$$ = 'gdnsip'}
             ;

TEXTFIELD  : 'asn.src'         {$$ = 'as1'}
           | 'asn.dst'         {$$ = 'as2'}
           | 'asn.dns'         {$$ = 'asdnsip'}
           | 'cert.subject.on' {$$ = 'tls.sOn'}
           | 'cert.issuer.on'  {$$ = 'tls.iOn'}
           ;

IPFIELD  : 'ip' {$$ = 0}
         | 'ip.src' {$$ = 1}
         | 'ip.dst' {$$ = 2}
         | 'ip.xff' {$$ = 3}
         | 'ip.dns' {$$ = 4}
         ;

STR : ID
    | asn
    | asn.dns
    | asn.dst
    | asn.src
    | asn.xff
    | bytes
    | cert.alt
    | cert.alt.cnt
    | cert.cnt
    | cert.issuer.cn
    | cert.issuer.on
    | cert.serial
    | cert.subject.cn
    | cert.subject.on
    | country
    | country.dns
    | country.dst
    | country.src
    | country.xff
    | header
    | header.dst
    | header.dst.cnt
    | header.src
    | header.src.cnt
    | host
    | host.cnt
    | icmp
    | ip
    | ip.dns
    | ip.dns.cnt
    | ip.dst
    | ip.src
    | ip.xff
    | ip.xff.cnt
    | node
    | packets
    | port
    | port.dst
    | port.src
    | protocol
    | QUOTEDSTR
    | ssh.key
    | ssh.key.cnt
    | ssh.ver
    | ssh.ver.cnt
    | tags
    | tags.cnt
    | tcp
    | ua
    | ua.cnt
    | udp
    | uri
    | uri.cnt
    ;
 
e
    : e '&&' e
        {$$ = {and: [$1, $3]};}
    | 'uri' '==' STR
        {$$ = {query: {text: {us: {query: $3, type: "phrase", operator: "and"}}}};}
    | 'uri' '!=' STR
        {$$ = {not: {query: {text: {us: {query: $3, type: "phrase", operator: "and"}}}}};}
    | 'ua' '==' STR
        {$$ = {query: {text: {ua: {query: $3, type: "phrase", operator: "and"}}}};}
    | 'ua' '!=' STR
        {$$ = {not: {query: {text: {ua: {query: $3, type: "phrase", operator: "and"}}}}};}
    | e '||' e
        {$$ = {or: [$1, $3]};}
    | '!' e %prec UMINUS
        {$$ = {not: $2};}
    | '-' e %prec UMINUS
        {$$ = -$2;}
    | '(' e ')'
        {$$ = $2;}
    | protocol '==' 'icmp'
        {$$ = {term: {pr: 1}};}
    | protocol '==' 'tcp'
        {$$ = {term: {pr: 6}};}
    | protocol '==' 'udp'
        {$$ = {term: {pr: 17}};}
    | protocol '!=' 'icmp'
        {$$ = {not: {term: {pr: 1}}};}
    | protocol '!=' 'tcp'
        {$$ = {not: {term: {pr: 6}}};}
    | protocol '!=' 'udp'
        {$$ = {not: {term: {pr: 17}}};}
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
    | LOTERMFIELD '!=' STR
        { var str = stripQuotes($3).toLowerCase();
          if (str.indexOf("*") !== -1) {
            $$ = {not: {query: {wildcard: {}}}};
            $$.not.query.wildcard[$1] = str;
          } else {
            $$ = {not: {term: {}}};
            $$.not.term[$1] = str;
          }
        }
    | LOTERMFIELD '==' STR
        { var str = stripQuotes($3).toLowerCase();
          if (str.indexOf("*") !== -1) {
            $$ = {query: {wildcard: {}}};
            $$.query.wildcard[$1] = str;
          } else {
            $$ = {term: {}};
            $$.term[$1] = str;
          }
        }
    | TERMFIELD '!=' STR
        { var str = stripQuotes($3);
          if (str.indexOf("*") !== -1) {
            $$ = {not: {query: {wildcard: {}}}};
            $$.not.query.wildcard[$1] = str;
          } else {
            $$ = {not: {term: {}}};
            $$.not.term[$1] = str;
          }
        }
    | TERMFIELD '==' STR
        { var str = stripQuotes($3);
          if (str.indexOf("*") !== -1) {
            $$ = {query: {wildcard: {}}};
            $$.query.wildcard[$1] = str;
          } else {
            $$ = {term: {}};
            $$.term[$1] = str;
          }
        }
    | UPTERMFIELD '!=' STR
        { var str = stripQuotes($3).toUpperCase();
          if (str.indexOf("*") !== -1) {
            $$ = {not: {query: {wildcard: {}}}};
            $$.not.query.wildcard[$1] = str;
          } else {
            $$ = {not: {term: {}}};
            $$.not.term[$1] = str;
          }
        }
    | UPTERMFIELD '==' STR
        { var str = stripQuotes($3).toUpperCase();
          if (str.indexOf("*") !== -1) {
            $$ = {query: {wildcard: {}}};
            $$.query.wildcard[$1] = str;
          } else {
            $$ = {term: {}};
            $$.term[$1] = str;
          }
        }
    | TEXTFIELD '!=' STR
        { var str = stripQuotes($3).toLowerCase();
          if (str.indexOf("*") !== -1) {
            $$ = {not: {query: {wildcard: {}}}};
            $$.not.query.wildcard[$1] = str;
          } else {
            $$ = {not: {query: {text: {}}}};
            $$.not.query.text[$1] = {query: str, type: "phrase", operator: "and"}
          }
        }
    | TEXTFIELD '==' STR
        { var str = stripQuotes($3).toLowerCase();
          if (str.indexOf("*") !== -1) {
            $$ = {query: {wildcard: {}}};
            $$.query.wildcard[$1] = str;
          } else {
            $$ = {query: {text: {}}};
            $$.query.text[$1] = {query: str, type: "phrase", operator: "and"}
          }
        }
    | 'port' '==' NUMBER
        {$$ = {or: [{term: {p1: $3}}, {term: {p2: $3}}]};}
    | 'port' '!=' NUMBER
        {$$ = {not: {or: [{term: {p1: $3}}, {term: {p2: $3}}]}};}
    | IPFIELD '==' IPNUM
        {$$ = parseIpPort($3,$1);}
    | IPFIELD '!=' IPNUM
        {$$ = {not: parseIpPort($3,$1)};}
    | tags '==' STR
        { var tag = stripQuotes($3);
          $$ = {term: {ta: tag}};
        }
    | tags '!=' STR
        { var tag = stripQuotes($3);
          $$ = {not: {term: {ta: tag}}};
        }
    | oldheader '==' STR
        { var tag = stripQuotes($3);
          $$ = {term: {hh: tag}};
        }
    | header '==' STR
        { var tag = stripQuotes($3);
          $$ = {or: [{term: {hh1: tag}}, {term:{hh2: tag}}]};
        }
    | 'header.src' '==' STR
        { var tag = stripQuotes($3);
          $$ = {term: {hh1: tag}};
        }
    | 'header.dst' '==' STR
        { var tag = stripQuotes($3);
          $$ = {term: {hh2: tag}};
        }
    | oldheader '!=' STR
        { var tag = stripQuotes($3);
          $$ = {not: {term: {hh1: tag}}};
        }
    | header '!=' STR
        { var tag = stripQuotes($3);
          $$ = {not: {or: [{term: {hh1: tag}}, {term:{hh2: tag}}]}};
        }
    | 'header.src' '!=' STR
        { var tag = stripQuotes($3);
          $$ = {not: {term: {hh1: tag}}};
        }
    | 'header.dst' '!=' STR
        { var tag = stripQuotes($3);
          $$ = {not: {term: {hh2: tag}}};
        }
    | country '==' STR 
        { var str = stripQuotes($3).toUpperCase();
          if (str.indexOf("*") !== -1) {
            $$ = {or: [{query: {wildcard: {g1: str}}}, {query: {wildcard: {g2: str}}}, {query: {wildcard: {gxff: str}}}]};
          } else {
            $$ = {or: [{term: {g1: str}}, {term: {g2: str}}, {term: {gxff: str}}]};
          }
        }
    | country '!=' STR 
        { var str = stripQuotes($3).toUpperCase();
          if (str.indexOf("*") !== -1) {
            $$ = {not: {or: [{query: {wildcard: {g1: str}}}, {query: {wildcard: {g2: str}}}, {query: {wildcard: {gxff: str}}}]}};
          } else {
            $$ = {not: {or: [{term: {g1: str}}, {term: {g2: str}}, {term: {gxff: str}}]}};
          }
        }
    | asn '==' STR 
        { var str = stripQuotes($3).toLowerCase();
          if (str.indexOf("*") !== -1) {
            $$ = {or: [{query: {wildcard: {as1: str}}}, {query: {wildcard: {as2: str}}}, {query: {wildcard: {asxff: str}}}]};
          } else {
            $$ = {or: [{query: {text: {as1:   {query: str, type: "phrase", operator: "and"}}}}, 
                       {query: {text: {as2:   {query: str, type: "phrase", operator: "and"}}}}, 
                       {query: {text: {asxff: {query: str, type: "phrase", operator: "and"}}}}
                      ]
                 };
          }
        }
    | asn '!=' STR 
        { var str = stripQuotes($3).toLowerCase();
          if (str.indexOf("*") !== -1) {
            $$ = {not: {or: [{query: {wildcard: {as1: str}}}, {query: {wildcard: {as2: str}}}, {query: {wildcard: {asxff: str}}}]}};
          } else {
            $$ = {not: {or: [{query: {text: {as1:   {query: str, type: "phrase", operator: "and"}}}}, 
                             {query: {text: {as2:   {query: str, type: "phrase", operator: "and"}}}}, 
                             {query: {text: {asxff: {query: str, type: "phrase", operator: "and"}}}}
                            ]
                 }};
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
  var dns;

  if (ip1 !== -1) {
    if (ip1 === ip2) {
        t1.and.push({term: {a1: ip1>>>0}});
        t2.and.push({term: {a2: ip1>>>0}});
        xff = {term: {xff: ip1>>>0}};
        dns = {term: {dnsip: ip1>>>0}};
    } else {
        t1.and.push({range: {a1: {from: ip1>>>0, to: ip2>>>0}}});
        t2.and.push({range: {a2: {from: ip1>>>0, to: ip2>>>0}}});
        xff =  {range: {xff: {from: ip1>>>0, to: ip2>>>0}}};
        dns =  {range: {dnsip: {from: ip1>>>0, to: ip2>>>0}}};
    }
  }

  if (port !== -1) {
    t1.and.push({term: {p1: port}});
    t2.and.push({term: {p2: port}});
  }

  if (t1.and.length === 1) {
      t1 = t1.and[0];
      t2 = t2.and[0];
  }

  switch(which) {
  case 0:
    if (xff)
        return {or: [t1, t2, xff]};
    else
        return {or: [t1, t2]};
  case 1:
    return t1;
  case 2:
    return t2;
  case 3:
    if (!xff)
        throw "xff doesn't support port only";
    return xff;
  case 4:
    if (!dns)
        throw "dns doesn't support port only";
    return dns;
  }
}

function stripQuotes (str) {
  if (str[0] === "\"") {
    str =  str.substring(1, str.length-1);
  }
  return str;
}
