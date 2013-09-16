/* lexical grammar */
%lex

%options flex
%%

\s+                   /* skip whitespace */
[0-9]+\b                  return 'NUMBER'
\[[0-9,'"]+\]             return 'NUMBERLIST'
([0-9]{1,3})?("."[0-9]{1,3})?("."[0-9]{1,3})?("."[0-9]{1,3})?("/"[0-9]{1,2})?(":"[0-9]{1,5})?\b return 'IPMATCH'
\[(?:([0-9]{1,3})?("."[0-9]{1,3})?("."[0-9]{1,3})?("."[0-9]{1,3})?("/"[0-9]{1,2})?(":"[0-9]{1,5})?[ ,]{0,1})+\] return 'IPMATCHLIST'

/* Backwards Names, to be removed */
"email.ct.cnt"            if (!yy.emailSearch) throw "email searches disabled for user"; return 'email.content-type.cnt'
"email.ct"                if (!yy.emailSearch) throw "email searches disabled for user"; return 'email.content-type'
"email.mv.cnt"            if (!yy.emailSearch) throw "email searches disabled for user"; return 'email.mime-version.cnt'
"email.mv"                if (!yy.emailSearch) throw "email searches disabled for user"; return 'email.mime-version'
"email.id.cnt"            if (!yy.emailSearch) throw "email searches disabled for user"; return 'email.message-id.cnt'
"email.id"                if (!yy.emailSearch) throw "email searches disabled for user"; return 'email.message-id'
"email.ua.cnt"            if (!yy.emailSearch) throw "email searches disabled for user"; return 'email.x-mailer.cnt'
"email.ua"                if (!yy.emailSearch) throw "email searches disabled for user"; return 'email.x-mailer'
"header.dst.cnt"          return "http.hasheader.dst.cnt"
"header.dst"              return "http.hasheader.dst"
"header"                  return "http.hasheader"
"header.src.cnt"          return "http.hasheader.src.cnt"
"header.src"              return "http.hasheader.src"
"http.ua.cnt"             return "http.user-agent.cnt"
"http.ua"                 return "http.user-agent"
"ua.cnt"                  return "http.user-agent.cnt"
"ua"                      return "http.user-agent"

/* Search Elements*/
"asn"                     return 'asn'
"asn.dns"                 return 'asn.dns'
"asn.dst"                 return 'asn.dst'
"asn.src"                 return 'asn.src'
"asn.xff"                 return 'asn.xff'
"asn.email"               if (!yy.emailSearch) throw "email searches disabled for user"; return 'asn.email'
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
"country.email"           if (!yy.emailSearch) throw "email searches disabled for user"; return 'country.email'
"databytes"               return 'databytes'
"email.content-type.cnt"  if (!yy.emailSearch) throw "email searches disabled for user"; return 'email.content-type.cnt'
"email.content-type"      if (!yy.emailSearch) throw "email searches disabled for user"; return 'email.content-type'
"email.dst.cnt"           if (!yy.emailSearch) throw "email searches disabled for user"; return 'email.dst.cnt'
"email.dst"               if (!yy.emailSearch) throw "email searches disabled for user"; return 'email.dst'
"email.fn.cnt"            if (!yy.emailSearch) throw "email searches disabled for user"; return 'email.fn.cnt'
"email.fn"                if (!yy.emailSearch) throw "email searches disabled for user"; return 'email.fn'
"email.message-id.cnt"    if (!yy.emailSearch) throw "email searches disabled for user"; return 'email.message-id.cnt'
"email.message-id"        if (!yy.emailSearch) throw "email searches disabled for user"; return 'email.message-id'
"email.md5.cnt"           if (!yy.emailSearch) throw "email searches disabled for user"; return 'email.md5.cnt'
"email.md5"               if (!yy.emailSearch) throw "email searches disabled for user"; return 'email.md5'
"email.mime-version.cnt"  if (!yy.emailSearch) throw "email searches disabled for user"; return 'email.mime-version.cnt'
"email.mime-version"      if (!yy.emailSearch) throw "email searches disabled for user"; return 'email.mime-version'
"email.src.cnt"           if (!yy.emailSearch) throw "email searches disabled for user"; return 'email.src.cnt'
"email.src"               if (!yy.emailSearch) throw "email searches disabled for user"; return 'email.src'
"email.subject.cnt"       if (!yy.emailSearch) throw "email searches disabled for user"; return 'email.subject.cnt'
"email.subject"           if (!yy.emailSearch) throw "email searches disabled for user"; return 'email.subject'
"email.x-mailer.cnt"      if (!yy.emailSearch) throw "email searches disabled for user"; return 'email.x-mailer.cnt'
"email.x-mailer"          if (!yy.emailSearch) throw "email searches disabled for user"; return 'email.x-mailer'
email\.[^\s!=><.]*\.cnt   if (!yy.emailSearch) throw "email searches disabled for user"; return 'EMAIL_HEADER_CNT'
email\.[^\s!=><.]*        if (!yy.emailSearch) throw "email searches disabled for user"; return 'EMAIL_HEADER'
"file"                    return "file"
"http.hasheader.dst.cnt"  return "http.hasheader.dst.cnt"
"http.hasheader.dst"      return "http.hasheader.dst"
"http.hasheader"          return "http.hasheader"
"http.hasheader.src.cnt"  return "http.hasheader.src.cnt"
"http.hasheader.src"      return "http.hasheader.src"
"host.dns.cnt"            return "host.dns.cnt"
"host.dns"                return "host.dns"
"host.email.cnt"          return "host.email.cnt"
"host.email"              return "host.email"
"host.http.cnt"           return "host.http.cnt"
"host.http"               return "host.http"
"host"                    return "host"
"http.md5.cnt"            return "http.md5.cnt"
"http.md5"                return "http.md5"
"http.uri.cnt"            return "http.uri.cnt"
"http.uri"                return "http.uri"
"http.version"            return "http.version"
"http.version.src"        return "http.version.src"
"http.version.src.cnt"    return "http.version.src.cnt"
"http.version.dst"        return "http.version.dst"
"http.version.dst.cnt"    return "http.version.dst.cnt"
"http.user-agent.cnt"     return "http.user-agent.cnt"
"http.user-agent"         return "http.user-agent"
"http.host.cnt"           return "host.http.cnt"
"http.host"               return "host.http"
http\.[^\s!=><.]*\.cnt    return "HTTP_HEADER_CNT"
http\.[^\s!=><.]*         return "HTTP_HEADER"
"icmp"                    return "icmp"
"\"icmp\""                return "icmp"
"id"                      return "id"
"ip.dns.cnt"              return "ip.dns.cnt"
"ip.dns"                  return "ip.dns"
"ip.dst"                  return "ip.dst"
"ip"                      return "ip"
"ip.src"                  return "ip.src"
"ip.xff.cnt"              return "ip.xff.cnt"
"ip.xff"                  return "ip.xff"
"ip.email.cnt"            if (!yy.emailSearch) throw "email searches disabled for user"; return "ip.email.cnt"
"ip.email"                if (!yy.emailSearch) throw "email searches disabled for user"; return "ip.email"
"irc.nick"                return "irc.nick"
"irc.nick.cnt"            return "irc.nick.cnt"
"irc.channel"             return "irc.channel"
"irc.channel.cnt"         return "irc.channel.cnt"
"node"                    return 'node'
"packets"                 return 'packets'
"port.dst"                return 'port.dst'
"port"                    return 'port'
"port.src"                return 'port.src'
"protocol"                return 'protocol'
"rootId"                  return "rootId"
"ssh.key.cnt"             return "ssh.key.cnt"
"ssh.key"                 return "ssh.key"
"ssh.ver.cnt"             return "ssh.ver.cnt"
"ssh.ver"                 return "ssh.ver"
"tags.cnt"                return 'tags.cnt'
"tags"                    return 'tags'
"tcp"                     return "tcp"
"\"tcp\""                 return "tcp"
"udp"                     return "udp"
"\"udp\""                 return "udp"
"uri.cnt"                 return "http.uri.cnt"
"uri"                     return "http.uri"
"user.cnt"                return "user.cnt"
"user"                    return "user"
[/\w*._:-]+               return 'ID'
\"[^"\\]*(?:\\.[^"\\]*)*\" return 'QUOTEDSTR'
\/[^/\\]*(?:\\.[^/\\]*)*\/ return 'REGEXSTR'
\[[0-9a-z,"]+\]           return 'LOWERLIST'
\[[^\]]+\]                return 'ANYLIST'
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
     | IPMATCHLIST
     | NUMBER
     | NUMBERLIST
     ;

HEADER: EMAIL_HEADER
      | HTTP_HEADER
      ;

HEADER_CNT: EMAIL_HEADER_CNT
          | HTTP_HEADER_CNT
          ;

RANGEFIELD: databytes                {$$ = 'db'}
          | bytes                    {$$ = 'by'}
          | packets                  {$$ = 'pa'}
          | 'port.src'               {$$ = 'p1'}
          | 'port.dst'               {$$ = 'p2'}
          | 'http.uri.cnt'           {$$ = 'uscnt'}
          | 'cert.cnt'               {$$ = 'tlscnt'}
          | 'ip.dns.cnt'             {$$ = 'dnsipcnt'}
          | 'ip.email.cnt'           {$$ = 'eipcnt'}
          | 'ip.xff.cnt'             {$$ = 'xffscnt'}
          | 'http.md5.cnt'           {$$ = 'hmd5cnt'}
          | 'http.user-agent.cnt'    {$$ = 'uacnt'}
          | 'http.version.src.cnt'   {$$ = 'hsvercnt'}
          | 'http.version.dst.cnt'   {$$ = 'hdvercnt'}
          | 'user.cnt'               {$$ = 'usercnt'}
          | 'host.dns.cnt'           {$$ = 'dnshocnt'}
          | 'host.email.cnt'         {$$ = 'ehocnt'}
          | 'host.http.cnt'          {$$ = 'hocnt'}
          | 'http.hasheader.src.cnt' {$$ = 'hh1cnt'}
          | 'http.hasheader.dst.cnt' {$$ = 'hh2cnt'}
          | 'tags.cnt'               {$$ = 'tacnt'}
          | 'email.content-type.cnt' {$$ = 'ectcnt'}
          | 'email.dst.cnt'          {$$ = 'edstcnt'}
          | 'email.fn.cnt'           {$$ = 'efncnt'}
          | 'email.message-id.cnt'   {$$ = 'eidcnt'}
          | 'email.md5.cnt'          {$$ = 'emd5cnt'}
          | 'email.mime-version.cnt' {$$ = 'emvcnt'}
          | 'email.src.cnt'          {$$ = 'esrccnt'}
          | 'email.subject.cnt'      {$$ = 'esubcnt'}
          | 'email.x-mailer.cnt'     {$$ = 'euacnt'}
          | 'cert.alt.cnt'           {$$ = 'tls.altcnt'}
          | 'ssh.key.cnt'            {$$ = 'sshkeycnt'}
          | 'ssh.ver.cnt'            {$$ = 'sshvercnt'}
          | 'irc.nick.cnt'           {$$ = 'ircnckcnt'}
          | 'irc.channel.cnt'        {$$ = 'ircchcnt'}
          ;

LOTERMFIELD  : node               {$$ = 'no'}
             | 'host.dns'         {$$ = 'dnsho'}
             | 'host.email'       {$$ = 'eho'}
             | 'host.http'        {$$ = 'ho'}
             | user               {$$ = 'user'}
             | 'email.dst'        {$$ = 'edst'}
             | 'email.src'        {$$ = 'esrc'}
             | 'cert.subject.cn'  {$$ = 'tls.sCn'}
             | 'cert.issuer.cn'   {$$ = 'tls.iCn'}
             | 'cert.serial'      {$$ = 'tls.sn'}
             | 'cert.alt'         {$$ = 'tls.alt'}
             | 'ssh.ver'          {$$ = 'sshver'}
             ;

TERMFIELD  : 'id'                 {$$ = '_id'}
           | 'ssh.key'            {$$ = 'sshkey'}
           | 'email.message-id'   {$$ = 'eid'}
           | 'email.md5'          {$$ = 'emd5'}
           | 'email.mime-version' {$$ = 'emv'}
           | 'email.fn'           {$$ = 'efn'}
           | 'email.content-type' {$$ = 'ect'}
           | 'http.md5'           {$$ = 'hmd5'}
           | 'http.version.src'   {$$ = 'hsver'}
           | 'http.version.dst'   {$$ = 'hdver'}
           | 'irc.nick'           {$$ = 'ircnck'}
           | 'irc.channel'        {$$ = 'ircch'}
           | 'rootId'             {$$ = 'ro'}
           ;

UPTERMFIELD  : 'country.src'   {$$ = 'g1'}
             | 'country.dst'   {$$ = 'g2'}
             | 'country.xff'   {$$ = 'gxff'}
             | 'country.email' {$$ = 'geip'}
             | 'country.dns'   {$$ = 'gdnsip'}
             ;

LOTEXTFIELD  : 'asn.src'         {$$ = 'as1'}
             | 'asn.dst'         {$$ = 'as2'}
             | 'asn.dns'         {$$ = 'asdnsip'}
             | 'asn.xff'         {$$ = 'asxff'}
             | 'asn.email'       {$$ = 'aseip'}
             | 'email.subject'   {$$ = 'esub'}
             | 'email.x-mailer'  {$$ = 'eua'}
             | 'cert.subject.on' {$$ = 'tls.sOn'}
             | 'cert.issuer.on'  {$$ = 'tls.iOn'}
             ;

TEXTFIELD  : 'http.uri'        {$$ = 'us'}
           | 'http.user-agent' {$$ = 'ua'}
           ;

IPFIELD  : 'ip'       {$$ = 0}
         | 'ip.src'   {$$ = 1}
         | 'ip.dst'   {$$ = 2}
         | 'ip.xff'   {$$ = 3}
         | 'ip.dns'   {$$ = 4}
         | 'ip.email' {$$ = 5}
         ;

STR : ID
    | asn
    | asn.dns
    | asn.dst
    | asn.email
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
    | country.email
    | country.src
    | country.xff
    | email.dst
    | email.dst.cnt
    | email.src
    | email.src.cnt
    | email.subject
    | email.subject.cnt
    | email.ua
    | email.ua.cnt
    | header
    | header.dst
    | header.dst.cnt
    | header.src
    | header.src.cnt
    | host
    | host.cnt
    | http.md5
    | http.md5.cnt
    | http.ua
    | http.ua.cnt
    | http.uri
    | http.uri.cnt
    | icmp
    | ip
    | ip.dns
    | ip.dns.cnt
    | ip.dst
    | ip.email
    | ip.email.cnt
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
    | REGEXSTR
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
    | LOWERLIST
    | ANYLIST
    | NUMBERLIST
    | IPMATCH
    | IPMATCHLIST
    ;

PROTOCOLNUMBER : NUMBER
               | icmp
               | tcp
               | udp
               ;

PROTOCOLLIST : NUMBERLIST
             | LOWERLIST
             ;
  
 
e
    : e '&&' e
        {$$ = {bool: {must: [$1, $3]}};}
    | e '||' e
        {$$ = {bool: {should: [$1, $3]}};}
    | '!' e %prec UMINUS
        {$$ = {not: $2};}
    | '-' e %prec UMINUS
        {$$ = -$2;}
    | '(' e ')'
        {$$ = $2;}
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
    | RANGEFIELD '==' NUMBERLIST
        {$$ = {terms: {}};
         $$.terms[$1] = CSVtoArray($3);}
    | RANGEFIELD '!=' NUMBERLIST
        {$$ = {not: {terms: {}}};
         $$.not.terms[$1] = CSVtoArray($3);}
    | protocol '==' PROTOCOLNUMBER
        {$$ = {term: {pr: protocolLookup($3)}};}
    | protocol '==' PROTOCOLLIST
        {$$ = {terms: {pr: protocolLookup(CSVtoArray($3))}};}
    | protocol '!=' PROTOCOLNUMBER
        {$$ = {not: {term: {pr: protocolLookup($3)}}};}
    | protocol '!=' PROTOCOLLIST
        {$$ = {not: {terms: {pr: protocolLookup(CSVtoArray($3))}}};}
    | 'port' GTLT NUMBER
        {$$ = {bool: {should: [{range: {p1: {}}}, {range: {p2: {}}}]}};
         $$.bool.should[0].range.p1[$2] = $3;
         $$.bool.should[1].range.p2[$2] = $3;}
    | LOTERMFIELD '!=' STR
        {$$ = {not: str2Query($1, "term", $3.toLowerCase())};}
    | LOTERMFIELD '==' STR
        {$$ = str2Query($1, "term", $3.toLowerCase());}
    | TERMFIELD '!=' STR
        {$$ = {not: str2Query($1, "term", $3)};}
    | TERMFIELD '==' STR
        {$$ = str2Query($1, "term", $3);}
    | UPTERMFIELD '!=' STR
        {$$ = {not: str2Query($1, "term", $3.toUpperCase())};}
    | UPTERMFIELD '==' STR
        {$$ = str2Query($1, "term", $3.toUpperCase());}
    | LOTEXTFIELD '!=' STR
        {$$ = {not: str2Query($1, "text", $3.toLowerCase())};}
    | LOTEXTFIELD '==' STR
        {$$ = str2Query($1, "text", $3.toLowerCase());}
    | TEXTFIELD '!=' STR
        {$$ = {not: str2Query($1, "text", $3)};}
    | TEXTFIELD '==' STR
        {$$ = str2Query($1, "text", $3);}
    | HEADER '==' STR
        {$$ = str2Query(str2Header(yy, $1), "text", $3);}
    | HEADER '!=' STR
        {$$ = {not: str2Query(str2Header(yy, $1), "text", $3)};}
    | HEADER '==' NUMBER
        {
        $$ = {term: {}};
        $$.term[str2Header(yy, $1)] = $3;
        }
    | HEADER '!=' NUMBER
        { $$ = {not: {term: {}}};
          $$.not.term[str2Header(yy, $1)] = $3;
        }
    | HEADER GTLT NUMBER
        {$$ = {range: {}};
         $$.range[str2Header(yy, $1)] = {};
         $$.range[str2Header(yy, $1)][$2] = $3;}
    | HEADER_CNT '==' NUMBER
        {
        $$ = {term: {}};
        $$.term[str2Header(yy, $1)] = $3;
        }
    | HEADER_CNT '!=' NUMBER
        { $$ = {not: {term: {}}};
          $$.not.term[str2Header(yy, $1)] = $3;
        }
    | HEADER_CNT '==' NUMBERLIST
        {
        $$ = {terms: {}};
        $$.terms[str2Header(yy, $1)] = $3;
        }
    | HEADER_CNT '!=' NUMBERLIST
        { $$ = {not: {terms: {}}};
          $$.not.terms[str2Header(yy, $1)] = $3;
        }
    | HEADER_CNT GTLT NUMBER
        {$$ = {range: {}};
         $$.range[str2Header(yy, $1)] = {};
         $$.range[str2Header(yy, $1)][$2] = $3;}
    | 'port' '==' NUMBER
        {$$ = {bool: {should: [{term: {p1: $3}}, {term: {p2: $3}}]}};}
    | 'port' '==' NUMBERLIST
        {$$ = {bool: {should: [{terms: {p1: CSVtoArray($3)}}, {terms: {p2: CSVtoArray($3)}}]}};}
    | 'port' '!=' NUMBER
        {$$ = {bool: {must_not: [{term: {p1: $3}}, {term: {p2: $3}}]}};}
    | 'port' '!=' NUMBERLIST
        {$$ = {bool: {must_not: [{terms: {p1: CSVtoArray($3)}}, {terms: {p2: CSVtoArray($3)}}]}};}
    | IPFIELD '==' IPNUM
        {$$ = parseIpPort(yy, $3,$1);}
    | IPFIELD '!=' IPNUM
        {$$ = {not: parseIpPort(yy, $3,$1)};}
    | tags '==' STR
        { var tag = stripQuotes($3);
          $$ = termOrTerms("ta", tag);
        }
    | tags '!=' STR
        { var tag = stripQuotes($3);
          $$ = {not: termOrTerms("ta", tag)};
        }
    | file '==' STR
        { var file = stripQuotes($3);
          $$ = {fileand: file};
        }
    | file '!=' STR
        { var file = stripQuotes($3);
          $$ = {not: {fileand: file}};
        }
    | 'http.hasheader' '==' STR
        { var tag = stripQuotes($3);
          $$ = {bool: {should: [termOrTerms("hh1", tag), termOrTerms("hh2", tag)]}};
        }
    | 'http.hasheader.src' '==' STR
        { var tag = stripQuotes($3);
          $$ = termOrTerms("hh1", tag);
        }
    | 'http.hasheader.dst' '==' STR
        { var tag = stripQuotes($3);
          $$ = termOrTerms("hh2", tag);
        }
    | 'http.hasheader' '!=' STR
        { var tag = stripQuotes($3);
          $$ = {bool: {must_not: [termOrTerms("hh1", tag), termOrTerms("hh2", tag)]}};
        }
    | 'http.hasheader.src' '!=' STR
        { var tag = stripQuotes($3);
          $$ = {not: termOrTerms("hh1", tag)};
        }
    | 'http.hasheader.dst' '!=' STR
        { var tag = stripQuotes($3);
          $$ = {not: termOrTerms("hh2", tag)};
        }
    | 'http.version' '==' STR 
        {
          $$ = [str2Query("hsver", "term", $3),
                str2Query("hdver", "term", $3)
               ];
          $$ = {bool: {should: $$}};
        }
    | 'http.version' '!=' STR 
        {
          $$ = [str2Query("hsver", "term", $3),
                str2Query("hdver", "term", $3)
               ];
          $$ = {bool: {must_not: $$}};
        }
    | country '==' STR 
        { var str = $3.toUpperCase();
          $$ = [str2Query("g1", "term", str),
                str2Query("g2", "term", str),
                str2Query("gxff", "term", str),
                str2Query("gdnsip", "term", str),
                str2Query("geip", "term", str)
               ];
          $$ = {bool: {should: $$}};
        }
    | country '!=' STR 
        { var str = $3.toUpperCase();
          $$ = [str2Query("g1", "term", str),
                str2Query("g2", "term", str),
                str2Query("gxff", "term", str),
                str2Query("gdnsip", "term", str),
                str2Query("geip", "term", str)
               ];
          $$ = {bool: {must_not: $$}};
        }
    | asn '==' STR 
        { var str = $3.toLowerCase();
          $$ = [str2Query("as1", "text", str),
                str2Query("as2", "text", str),
                str2Query("asxff", "text", str),
                str2Query("asdnsip", "text", str),
                str2Query("aseip", "text", str)
               ];
          $$ = {bool: {should: $$}};
        }
    | asn '!=' STR 
        { var str = $3.toLowerCase();
          $$ = [str2Query("as1", "text", str),
                str2Query("as2", "text", str),
                str2Query("asxff", "text", str),
                str2Query("asdnsip", "text", str),
                str2Query("aseip", "text", str)
               ];
          $$ = {bool: {must_not: $$}};
        }
    | host '!=' STR
        { var str = $3.toLowerCase();

          $$ = [str2Query("ho", "term", str),
                str2Query("dnsho", "term", str),
                str2Query("eho", "term", str)
               ];
          $$ = {bool: {must_not: $$}};
        }
    | host '==' STR
        { var str = $3.toLowerCase();

          $$ = [str2Query("ho", "term", str),
                str2Query("dnsho", "term", str),
                str2Query("eho", "term", str)
               ];
          $$ = {bool: {should: $$}};
        }
    ;
%%

var    util           = require('util');

function parseIpPort(yy, ipPortStr, which) {
  ipPortStr = ipPortStr.trim();

// We really have a list of them
  if (ipPortStr[0] === "[" && ipPortStr[ipPortStr.length -1] === "]") {
      var obj =  {bool: {should: []}};
      CSVtoArray(ipPortStr).forEach(function(str) {
          obj.bool.should.push(parseIpPort(yy, str, which));
      });
      return obj;
  }

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

  var t1 = {bool: {must: []}};
  var t2 = {bool: {must: []}};
  var xff;
  var dns;
  var email;

  if (ip1 !== -1) {
    if (ip1 === ip2) {
        t1.bool.must.push({term: {a1: ip1>>>0}});
        t2.bool.must.push({term: {a2: ip1>>>0}});
        dns   = {term: {dnsip: ip1>>>0}};
        email = {term: {eip: ip1>>>0}};
        xff   = {term: {xff: ip1>>>0}};
    } else {
        t1.bool.must.push({range: {a1: {from: ip1>>>0, to: ip2>>>0}}});
        t2.bool.must.push({range: {a2: {from: ip1>>>0, to: ip2>>>0}}});
        dns =    {range: {dnsip: {from: ip1>>>0, to: ip2>>>0}}};
        email =  {range: {eip: {from: ip1>>>0, to: ip2>>>0}}};
        xff =    {range: {xff: {from: ip1>>>0, to: ip2>>>0}}};
    }
  }

  if (port !== -1) {
    t1.bool.must.push({term: {p1: port}});
    t2.bool.must.push({term: {p2: port}});
  }

  if (t1.bool.must.length === 1) {
      t1 = t1.bool.must[0];
      t2 = t2.bool.must[0];
  }

  switch(which) {
  case 0:
    var ors = [t1, t2];

    if (xff)
        ors.push(xff);
    if (dns)
        ors.push(dns);
    if (yy.emailSearch === true && email)
        ors.push(email);

    return {bool: {should: ors}};
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
  case 5:
    if (!email)
        throw "email doesn't support port only";
    return email;
  }
}

function stripQuotes (str) {
  if (str[0] === "\"") {
    str =  str.substring(1, str.length-1);
  }
  return str;
}

var field2RawField = {
    us: "rawus",
    ua: "rawua",
    as1: "rawas1",
    as2: "rawas2",
    asxff: "rawasff",
    asdnsip: "rawasdnsip",
    iOn: "rawiOn",
    eua: "raweua",
    esub: "rawesub",
    aseip: "rawaseip"
}

function str2Query(field, kind, str)
{
    var obj;

    if (field2RawField[field] === undefined && field.indexOf(".snow", field.length - 5) !== -1) {
        field2RawField[field] = field.substring(0, field.length - 5) + ".raw";
    }

    if (str[0] === "/" && str[str.length -1] === "/") {
        field = field2RawField[field] || field;
        obj = {regexp: {}};
        obj.regexp[field] = str.substring(1, str.length-1).replace(/\\(.)/g, "$1");
        return obj;
    }

    var strs;
    if (str[0] === "\"" && str[str.length -1] === "\"") {
        str = str.substring(1, str.length-1).replace(/\\(.)/g, "$1");
    } else if (str[0] === "[" && str[str.length -1] === "]") {
        strs = CSVtoArray(str);
        if (kind === "term") {
            obj = {terms: {}};
            obj.terms[field] = strs;
        } else if (kind === "text") {
            var obj =  {query: {bool: {should: []}}};
            strs.forEach(function(str) {
              var should = {text: {}};
              should.text[field] = {query: str, type: "phrase", operator: "and"}
              obj.query.bool.should.push(should);
            });
        }
        return obj;
    }

    if (str.indexOf("*") !== -1) {
        field = field2RawField[field] || field;
        obj = {query: {wildcard: {}}};
        obj.query.wildcard[field] = str;
    } else if (kind === "text") {
        obj = {query: {text: {}}};
        obj.query.text[field] = {query: str, type: "phrase", operator: "and"}
    } else if (kind === "term") {
        obj = {term: {}};
        obj.term[field] = str;
    }
    return obj;
}

function str2Header(yy, name) {
    var field = yy.fieldsMap[name];
    if (field === undefined) throw "Unknown field " + name;

    return field.db;
}

// http://stackoverflow.com/a/8497474
// Return array of string values, or NULL if CSV string not well formed.
function CSVtoArray(text) {
    text = text.substring(1, text.length-1);
    var re_valid = /^\s*(?:'[^'\\]*(?:\\[\S\s][^'\\]*)*'|"[^"\\]*(?:\\[\S\s][^"\\]*)*"|[^,'"\s\\]*(?:\s+[^,'"\s\\]+)*)\s*(?:,\s*(?:'[^'\\]*(?:\\[\S\s][^'\\]*)*'|"[^"\\]*(?:\\[\S\s][^"\\]*)*"|[^,'"\s\\]*(?:\s+[^,'"\s\\]+)*)\s*)*$/;
    var re_value = /(?!\s*$)\s*(?:'([^'\\]*(?:\\[\S\s][^'\\]*)*)'|"([^"\\]*(?:\\[\S\s][^"\\]*)*)"|([^,'"\s\\]*(?:\s+[^,'"\s\\]+)*))\s*(?:,|$)/g;
    // Return NULL if input string is not well formed CSV string.
    if (!re_valid.test(text)) return null;
    var a = [];                     // Initialize array to receive values.
    text.replace(re_value, // "Walk" the string using replace with callback.
        function(m0, m1, m2, m3) {
            // Remove backslash from \' in single quoted values.
            if      (m1 !== undefined) a.push(m1.replace(/\\'/g, "'"));
            // Remove backslash from \" in double quoted values.
            else if (m2 !== undefined) a.push(m2.replace(/\\"/g, '"'));
            else if (m3 !== undefined) a.push(m3);
            return ''; // Return empty string.
        });
    // Handle special case of empty last value.
    if (/,\s*$/.test(text)) a.push('');
    return a;
};

var protocols = {
    icmp: 1,
    tcp:  6,
    udp:  17
};

function protocolLookup(text) {
    if (typeof text !== "string") {
        for (var i = 0; i < text.length; i++) {
            text[i] = protocols[text[i]] || +text[i];
        }
        return text;
    } else {
        return protocols[text] || +text;
    }
}

function termOrTerms(field, str) {
  var obj = {};
  if (str[0] === "[" && str[str.length -1] === "]") {
    obj = {terms: {}};
    obj.terms[field] = CSVtoArray(str);
  } else {
    obj = {term: {}};
    obj.term[field] = str;
  }
  return obj;
}
