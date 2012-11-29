/* Jison generated parser */
var molochparser = (function(){
var parser = {trace: function trace() { },
yy: {},
symbols_: {"error":2,"expressions":3,"e":4,"EOF":5,"LTA":6,"lt":7,"lte":8,"GTA":9,"gt":10,"gte":11,"GTLT":12,"IPNUM":13,"IPMATCH":14,"NUMBER":15,"RANGEFIELD":16,"databytes":17,"bytes":18,"packets":19,"protocol":20,"port.src":21,"port.dst":22,"uri.cnt":23,"cert.cnt":24,"ip.xff.cnt":25,"ua.cnt":26,"user.cnt":27,"host.cnt":28,"header.src.cnt":29,"header.dst.cnt":30,"tags.cnt":31,"cert.alt.cnt":32,"TERMFIELD":33,"node":34,"host":35,"user":36,"cert.subject.cn":37,"cert.issuer.cn":38,"cert.serial":39,"cert.alt":40,"UPTERMFIELD":41,"country.src":42,"country.dst":43,"country.xff":44,"TEXTFIELD":45,"asn.src":46,"asn.dst":47,"asn.xff":48,"cert.subject.on":49,"cert.issuer.on":50,"STR":51,"ID":52,"port":53,"src":54,"dst":55,"country":56,"xff":57,"asn":58,"cert":59,"issuer":60,"cn":61,"on":62,"subject":63,"alt":64,"cnt":65,"serial":66,"QUOTEDSTR":67,"header":68,"icmp":69,"tcp":70,"udp":71,"ip":72,"uri":73,"ua":74,"tags":75,"&&":76,"==":77,"!=":78,"||":79,"!":80,"-":81,"(":82,")":83,"ip.src":84,"ip.dst":85,"ip.xff":86,"oldheader":87,"header.src":88,"header.dst":89,"$accept":0,"$end":1},
terminals_: {2:"error",5:"EOF",7:"lt",8:"lte",10:"gt",11:"gte",14:"IPMATCH",15:"NUMBER",17:"databytes",18:"bytes",19:"packets",20:"protocol",21:"port.src",22:"port.dst",23:"uri.cnt",24:"cert.cnt",25:"ip.xff.cnt",26:"ua.cnt",27:"user.cnt",28:"host.cnt",29:"header.src.cnt",30:"header.dst.cnt",31:"tags.cnt",32:"cert.alt.cnt",34:"node",35:"host",36:"user",37:"cert.subject.cn",38:"cert.issuer.cn",39:"cert.serial",40:"cert.alt",42:"country.src",43:"country.dst",44:"country.xff",46:"asn.src",47:"asn.dst",48:"asn.xff",49:"cert.subject.on",50:"cert.issuer.on",52:"ID",53:"port",54:"src",55:"dst",56:"country",57:"xff",58:"asn",59:"cert",60:"issuer",61:"cn",62:"on",63:"subject",64:"alt",65:"cnt",66:"serial",67:"QUOTEDSTR",68:"header",69:"icmp",70:"tcp",71:"udp",72:"ip",73:"uri",74:"ua",75:"tags",76:"&&",77:"==",78:"!=",79:"||",80:"!",81:"-",82:"(",83:")",84:"ip.src",85:"ip.dst",86:"ip.xff",87:"oldheader",88:"header.src",89:"header.dst"},
productions_: [0,[3,2],[6,1],[6,1],[9,1],[9,1],[12,1],[12,1],[13,1],[13,1],[16,1],[16,1],[16,1],[16,1],[16,1],[16,1],[16,1],[16,1],[16,1],[16,1],[16,1],[16,1],[16,1],[16,1],[16,1],[16,1],[33,1],[33,1],[33,1],[33,1],[33,1],[33,1],[33,1],[41,1],[41,1],[41,1],[45,1],[45,1],[45,1],[45,1],[45,1],[51,1],[51,1],[51,1],[51,1],[51,1],[51,2],[51,2],[51,1],[51,2],[51,2],[51,2],[51,1],[51,2],[51,2],[51,2],[51,3],[51,3],[51,3],[51,3],[51,2],[51,3],[51,2],[51,2],[51,1],[51,1],[51,1],[51,2],[51,1],[51,2],[51,3],[51,2],[51,3],[51,1],[51,1],[51,1],[51,1],[51,2],[51,2],[51,2],[51,3],[51,1],[51,2],[51,1],[51,2],[51,1],[51,2],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,2],[4,2],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3]],
performAction: function anonymous(yytext,yyleng,yylineno,yy,yystate,$$,_$) {

var $0 = $$.length - 1;
switch (yystate) {
case 1: return $$[$0-1]; 
break;
case 2:this.$ = 'lt'
break;
case 3:this.$ = 'lte'
break;
case 4:this.$ = 'gt'
break;
case 5:this.$ = 'gte'
break;
case 10:this.$ = 'db'
break;
case 11:this.$ = 'by'
break;
case 12:this.$ = 'pa'
break;
case 13:this.$ = 'pr'
break;
case 14:this.$ = 'p1'
break;
case 15:this.$ = 'p2'
break;
case 16:this.$ = 'uscnt'
break;
case 17:this.$ = 'tlscnt'
break;
case 18:this.$ = 'xffcnt'
break;
case 19:this.$ = 'uacnt'
break;
case 20:this.$ = 'usercnt'
break;
case 21:this.$ = 'hocnt'
break;
case 22:this.$ = 'hh1cnt'
break;
case 23:this.$ = 'hh2cnt'
break;
case 24:this.$ = 'tacnt'
break;
case 25:this.$ = 'tls.altcnt'
break;
case 26:this.$ = 'no'
break;
case 27:this.$ = 'ho'
break;
case 28:this.$ = 'user'
break;
case 29:this.$ = 'tls.sCn'
break;
case 30:this.$ = 'tls.iCn'
break;
case 31:this.$ = 'tls.sn'
break;
case 32:this.$ = 'tls.alt'
break;
case 33:this.$ = 'g1'
break;
case 34:this.$ = 'g2'
break;
case 35:this.$ = 'gxff'
break;
case 36:this.$ = 'as1'
break;
case 37:this.$ = 'as2'
break;
case 38:this.$ = 'asxff'
break;
case 39:this.$ = 'tls.sOn'
break;
case 40:this.$ = 'tls.iOn'
break;
case 87:this.$ = {and: [$$[$0-2], $$[$0]]};
break;
case 88:this.$ = {query: {text: {us: {query: $$[$0], type: "phrase", operator: "and"}}}};
break;
case 89:this.$ = {not: {query: {text: {us: {query: $$[$0], type: "phrase", operator: "and"}}}}};
break;
case 90:this.$ = {query: {text: {ua: {query: $$[$0], type: "phrase", operator: "and"}}}};
break;
case 91:this.$ = {not: {query: {text: {ua: {query: $$[$0], type: "phrase", operator: "and"}}}}};
break;
case 92:this.$ = {or: [$$[$0-2], $$[$0]]};
break;
case 93:this.$ = {not: $$[$0]};
break;
case 94:this.$ = -$$[$0];
break;
case 95:this.$ = $$[$0-1];
break;
case 96:this.$ = {term: {pr: 1}};
break;
case 97:this.$ = {term: {pr: 6}};
break;
case 98:this.$ = {term: {pr: 17}};
break;
case 99:this.$ = {not: {term: {pr: 1}}};
break;
case 100:this.$ = {not: {term: {pr: 6}}};
break;
case 101:this.$ = {not: {term: {pr: 17}}};
break;
case 102:this.$ = {range: {}};
         this.$.range[$$[$0-2]] = {};
         this.$.range[$$[$0-2]][$$[$0-1]] = $$[$0];
break;
case 103:this.$ = {term: {}};
         this.$.term[$$[$0-2]] = $$[$0];
break;
case 104:this.$ = {not: {term: {}}};
         this.$.not.term[$$[$0-2]] = $$[$0];
break;
case 105:this.$ = {or: [{range: {p1: {}}}, {range: {p2: {}}}]};
         this.$.or[0].range.p1[$$[$0-1]] = $$[$0];
         this.$.or[1].range.p2[$$[$0-1]] = $$[$0];
break;
case 106: var str = stripQuotes($$[$0]).toLowerCase();
          if (str.indexOf("*") !== -1) {
            this.$ = {not: {query: {wildcard: {}}}};
            this.$.not.query.wildcard[$$[$0-2]] = str;
          } else {
            this.$ = {not: {term: {}}};
            this.$.not.term[$$[$0-2]] = str;
          }
        
break;
case 107: var str = stripQuotes($$[$0]).toLowerCase();
          if (str.indexOf("*") !== -1) {
            this.$ = {query: {wildcard: {}}};
            this.$.query.wildcard[$$[$0-2]] = str;
          } else {
            this.$ = {term: {}};
            this.$.term[$$[$0-2]] = str;
          }
        
break;
case 108: var str = stripQuotes($$[$0]).toUpperCase();
          if (str.indexOf("*") !== -1) {
            this.$ = {not: {query: {wildcard: {}}}};
            this.$.not.query.wildcard[$$[$0-2]] = str;
          } else {
            this.$ = {not: {term: {}}};
            this.$.not.term[$$[$0-2]] = str;
          }
        
break;
case 109: var str = stripQuotes($$[$0]).toUpperCase();
          if (str.indexOf("*") !== -1) {
            this.$ = {query: {wildcard: {}}};
            this.$.query.wildcard[$$[$0-2]] = str;
          } else {
            this.$ = {term: {}};
            this.$.term[$$[$0-2]] = str;
          }
        
break;
case 110: var str = stripQuotes($$[$0]).toLowerCase();
          if (str.indexOf("*") !== -1) {
            this.$ = {not: {query: {wildcard: {}}}};
            this.$.not.query.wildcard[$$[$0-2]] = str;
          } else {
            this.$ = {not: {query: {text: {}}}};
            this.$.not.query.text[$$[$0-2]] = {query: str, type: "phrase", operator: "and"}
          }
        
break;
case 111: var str = stripQuotes($$[$0]).toLowerCase();
          if (str.indexOf("*") !== -1) {
            this.$ = {query: {wildcard: {}}};
            this.$.query.wildcard[$$[$0-2]] = str;
          } else {
            this.$ = {query: {text: {}}};
            this.$.query.text[$$[$0-2]] = {query: str, type: "phrase", operator: "and"}
          }
        
break;
case 112:this.$ = {or: [{term: {p1: $$[$0]}}, {term: {p2: $$[$0]}}]};
break;
case 113:this.$ = {not: {or: [{term: {p1: $$[$0]}}, {term: {p2: $$[$0]}}]}};
break;
case 114:this.$ = parseIpPort($$[$0],0);
break;
case 115:this.$ = {not: parseIpPort($$[$0],0)};
break;
case 116:this.$ = parseIpPort($$[$0],1);
break;
case 117:this.$ = {not: parseIpPort($$[$0],1)};
break;
case 118:this.$ = parseIpPort($$[$0],2);
break;
case 119:this.$ = {not: parseIpPort($$[$0],2)};
break;
case 120:this.$ = parseIpPort($$[$0],3);
break;
case 121:this.$ = {not: parseIpPort($$[$0],3)};
break;
case 122: var tag = stripQuotes($$[$0]);
          this.$ = {term: {ta: tag}};
        
break;
case 123: var tag = stripQuotes($$[$0]);
          this.$ = {not: {term: {ta: tag}}};
        
break;
case 124: var tag = stripQuotes($$[$0]);
          this.$ = {term: {hh: tag}};
        
break;
case 125: var tag = stripQuotes($$[$0]);
          this.$ = {or: [{term: {hh1: tag}}, {term:{hh2: tag}}]};
        
break;
case 126: var tag = stripQuotes($$[$0]);
          this.$ = {term: {hh1: tag}};
        
break;
case 127: var tag = stripQuotes($$[$0]);
          this.$ = {term: {hh2: tag}};
        
break;
case 128: var tag = stripQuotes($$[$0]);
          this.$ = {not: {term: {hh1: tag}}};
        
break;
case 129: var tag = stripQuotes($$[$0]);
          this.$ = {not: {or: [{term: {hh1: tag}}, {term:{hh2: tag}}]}};
        
break;
case 130: var tag = stripQuotes($$[$0]);
          this.$ = {not: {term: {hh1: tag}}};
        
break;
case 131: var tag = stripQuotes($$[$0]);
          this.$ = {not: {term: {hh2: tag}}};
        
break;
case 132: var str = stripQuotes($$[$0]).toUpperCase();
          if (str.indexOf("*") !== -1) {
            this.$ = {or: [{query: {wildcard: {g1: str}}}, {query: {wildcard: {g2: str}}}, {query: {wildcard: {gxff: str}}}]};
          } else {
            this.$ = {or: [{term: {g1: str}}, {term: {g2: str}}, {term: {gxff: str}}]};
          }
        
break;
case 133: var str = stripQuotes($$[$0]).toUpperCase();
          if (str.indexOf("*") !== -1) {
            this.$ = {not: {or: [{query: {wildcard: {g1: str}}}, {query: {wildcard: {g2: str}}}, {query: {wildcard: {gxff: str}}}]}};
          } else {
            this.$ = {not: {or: [{term: {g1: str}}, {term: {g2: str}}, {term: {gxff: str}}]}};
          }
        
break;
case 134: var str = stripQuotes($$[$0]).toLowerCase();
          if (str.indexOf("*") !== -1) {
            this.$ = {or: [{query: {wildcard: {as1: str}}}, {query: {wildcard: {as2: str}}}, {query: {wildcard: {asxff: str}}}]};
          } else {
            this.$ = {or: [{query: {text: {as1:   {query: str, type: "phrase", operator: "and"}}}}, 
                       {query: {text: {as2:   {query: str, type: "phrase", operator: "and"}}}}, 
                       {query: {text: {asxff: {query: str, type: "phrase", operator: "and"}}}}
                      ]
                 };
          }
        
break;
case 135: var str = stripQuotes($$[$0]).toLowerCase();
          if (str.indexOf("*") !== -1) {
            this.$ = {not: {or: [{query: {wildcard: {as1: str}}}, {query: {wildcard: {as2: str}}}, {query: {wildcard: {asxff: str}}}]}};
          } else {
            this.$ = {not: {or: [{query: {text: {as1:   {query: str, type: "phrase", operator: "and"}}}}, 
                             {query: {text: {as2:   {query: str, type: "phrase", operator: "and"}}}}, 
                             {query: {text: {asxff: {query: str, type: "phrase", operator: "and"}}}}
                            ]
                 }};
          }
        
break;
}
},
table: [{3:1,4:2,16:9,17:[1,25],18:[1,26],19:[1,27],20:[1,8],21:[1,28],22:[1,29],23:[1,30],24:[1,31],25:[1,32],26:[1,33],27:[1,34],28:[1,35],29:[1,36],30:[1,37],31:[1,38],32:[1,39],33:11,34:[1,40],35:[1,41],36:[1,42],37:[1,43],38:[1,44],39:[1,45],40:[1,46],41:12,42:[1,47],43:[1,48],44:[1,49],45:13,46:[1,50],47:[1,51],48:[1,52],49:[1,53],50:[1,54],53:[1,10],56:[1,23],58:[1,24],68:[1,20],72:[1,14],73:[1,3],74:[1,4],75:[1,18],80:[1,5],81:[1,6],82:[1,7],84:[1,15],85:[1,16],86:[1,17],87:[1,19],88:[1,21],89:[1,22]},{1:[3]},{5:[1,55],76:[1,56],79:[1,57]},{77:[1,58],78:[1,59]},{77:[1,60],78:[1,61]},{4:62,16:9,17:[1,25],18:[1,26],19:[1,27],20:[1,8],21:[1,28],22:[1,29],23:[1,30],24:[1,31],25:[1,32],26:[1,33],27:[1,34],28:[1,35],29:[1,36],30:[1,37],31:[1,38],32:[1,39],33:11,34:[1,40],35:[1,41],36:[1,42],37:[1,43],38:[1,44],39:[1,45],40:[1,46],41:12,42:[1,47],43:[1,48],44:[1,49],45:13,46:[1,50],47:[1,51],48:[1,52],49:[1,53],50:[1,54],53:[1,10],56:[1,23],58:[1,24],68:[1,20],72:[1,14],73:[1,3],74:[1,4],75:[1,18],80:[1,5],81:[1,6],82:[1,7],84:[1,15],85:[1,16],86:[1,17],87:[1,19],88:[1,21],89:[1,22]},{4:63,16:9,17:[1,25],18:[1,26],19:[1,27],20:[1,8],21:[1,28],22:[1,29],23:[1,30],24:[1,31],25:[1,32],26:[1,33],27:[1,34],28:[1,35],29:[1,36],30:[1,37],31:[1,38],32:[1,39],33:11,34:[1,40],35:[1,41],36:[1,42],37:[1,43],38:[1,44],39:[1,45],40:[1,46],41:12,42:[1,47],43:[1,48],44:[1,49],45:13,46:[1,50],47:[1,51],48:[1,52],49:[1,53],50:[1,54],53:[1,10],56:[1,23],58:[1,24],68:[1,20],72:[1,14],73:[1,3],74:[1,4],75:[1,18],80:[1,5],81:[1,6],82:[1,7],84:[1,15],85:[1,16],86:[1,17],87:[1,19],88:[1,21],89:[1,22]},{4:64,16:9,17:[1,25],18:[1,26],19:[1,27],20:[1,8],21:[1,28],22:[1,29],23:[1,30],24:[1,31],25:[1,32],26:[1,33],27:[1,34],28:[1,35],29:[1,36],30:[1,37],31:[1,38],32:[1,39],33:11,34:[1,40],35:[1,41],36:[1,42],37:[1,43],38:[1,44],39:[1,45],40:[1,46],41:12,42:[1,47],43:[1,48],44:[1,49],45:13,46:[1,50],47:[1,51],48:[1,52],49:[1,53],50:[1,54],53:[1,10],56:[1,23],58:[1,24],68:[1,20],72:[1,14],73:[1,3],74:[1,4],75:[1,18],80:[1,5],81:[1,6],82:[1,7],84:[1,15],85:[1,16],86:[1,17],87:[1,19],88:[1,21],89:[1,22]},{7:[2,13],8:[2,13],10:[2,13],11:[2,13],77:[1,65],78:[1,66]},{6:70,7:[1,72],8:[1,73],9:71,10:[1,74],11:[1,75],12:67,77:[1,68],78:[1,69]},{6:70,7:[1,72],8:[1,73],9:71,10:[1,74],11:[1,75],12:76,77:[1,77],78:[1,78]},{77:[1,80],78:[1,79]},{77:[1,82],78:[1,81]},{77:[1,84],78:[1,83]},{77:[1,85],78:[1,86]},{77:[1,87],78:[1,88]},{77:[1,89],78:[1,90]},{77:[1,91],78:[1,92]},{77:[1,93],78:[1,94]},{77:[1,95],78:[1,96]},{77:[1,97],78:[1,98]},{77:[1,99],78:[1,100]},{77:[1,101],78:[1,102]},{77:[1,103],78:[1,104]},{77:[1,105],78:[1,106]},{7:[2,10],8:[2,10],10:[2,10],11:[2,10],77:[2,10],78:[2,10]},{7:[2,11],8:[2,11],10:[2,11],11:[2,11],77:[2,11],78:[2,11]},{7:[2,12],8:[2,12],10:[2,12],11:[2,12],77:[2,12],78:[2,12]},{7:[2,14],8:[2,14],10:[2,14],11:[2,14],77:[2,14],78:[2,14]},{7:[2,15],8:[2,15],10:[2,15],11:[2,15],77:[2,15],78:[2,15]},{7:[2,16],8:[2,16],10:[2,16],11:[2,16],77:[2,16],78:[2,16]},{7:[2,17],8:[2,17],10:[2,17],11:[2,17],77:[2,17],78:[2,17]},{7:[2,18],8:[2,18],10:[2,18],11:[2,18],77:[2,18],78:[2,18]},{7:[2,19],8:[2,19],10:[2,19],11:[2,19],77:[2,19],78:[2,19]},{7:[2,20],8:[2,20],10:[2,20],11:[2,20],77:[2,20],78:[2,20]},{7:[2,21],8:[2,21],10:[2,21],11:[2,21],77:[2,21],78:[2,21]},{7:[2,22],8:[2,22],10:[2,22],11:[2,22],77:[2,22],78:[2,22]},{7:[2,23],8:[2,23],10:[2,23],11:[2,23],77:[2,23],78:[2,23]},{7:[2,24],8:[2,24],10:[2,24],11:[2,24],77:[2,24],78:[2,24]},{7:[2,25],8:[2,25],10:[2,25],11:[2,25],77:[2,25],78:[2,25]},{77:[2,26],78:[2,26]},{77:[2,27],78:[2,27]},{77:[2,28],78:[2,28]},{77:[2,29],78:[2,29]},{77:[2,30],78:[2,30]},{77:[2,31],78:[2,31]},{77:[2,32],78:[2,32]},{77:[2,33],78:[2,33]},{77:[2,34],78:[2,34]},{77:[2,35],78:[2,35]},{77:[2,36],78:[2,36]},{77:[2,37],78:[2,37]},{77:[2,38],78:[2,38]},{77:[2,39],78:[2,39]},{77:[2,40],78:[2,40]},{1:[2,1]},{4:107,16:9,17:[1,25],18:[1,26],19:[1,27],20:[1,8],21:[1,28],22:[1,29],23:[1,30],24:[1,31],25:[1,32],26:[1,33],27:[1,34],28:[1,35],29:[1,36],30:[1,37],31:[1,38],32:[1,39],33:11,34:[1,40],35:[1,41],36:[1,42],37:[1,43],38:[1,44],39:[1,45],40:[1,46],41:12,42:[1,47],43:[1,48],44:[1,49],45:13,46:[1,50],47:[1,51],48:[1,52],49:[1,53],50:[1,54],53:[1,10],56:[1,23],58:[1,24],68:[1,20],72:[1,14],73:[1,3],74:[1,4],75:[1,18],80:[1,5],81:[1,6],82:[1,7],84:[1,15],85:[1,16],86:[1,17],87:[1,19],88:[1,21],89:[1,22]},{4:108,16:9,17:[1,25],18:[1,26],19:[1,27],20:[1,8],21:[1,28],22:[1,29],23:[1,30],24:[1,31],25:[1,32],26:[1,33],27:[1,34],28:[1,35],29:[1,36],30:[1,37],31:[1,38],32:[1,39],33:11,34:[1,40],35:[1,41],36:[1,42],37:[1,43],38:[1,44],39:[1,45],40:[1,46],41:12,42:[1,47],43:[1,48],44:[1,49],45:13,46:[1,50],47:[1,51],48:[1,52],49:[1,53],50:[1,54],53:[1,10],56:[1,23],58:[1,24],68:[1,20],72:[1,14],73:[1,3],74:[1,4],75:[1,18],80:[1,5],81:[1,6],82:[1,7],84:[1,15],85:[1,16],86:[1,17],87:[1,19],88:[1,21],89:[1,22]},{18:[1,112],19:[1,111],20:[1,113],34:[1,119],35:[1,120],51:109,52:[1,110],53:[1,114],56:[1,115],58:[1,116],59:[1,117],67:[1,118],68:[1,121],69:[1,122],70:[1,123],71:[1,124],72:[1,125],73:[1,126],74:[1,127],75:[1,128]},{18:[1,112],19:[1,111],20:[1,113],34:[1,119],35:[1,120],51:129,52:[1,110],53:[1,114],56:[1,115],58:[1,116],59:[1,117],67:[1,118],68:[1,121],69:[1,122],70:[1,123],71:[1,124],72:[1,125],73:[1,126],74:[1,127],75:[1,128]},{18:[1,112],19:[1,111],20:[1,113],34:[1,119],35:[1,120],51:130,52:[1,110],53:[1,114],56:[1,115],58:[1,116],59:[1,117],67:[1,118],68:[1,121],69:[1,122],70:[1,123],71:[1,124],72:[1,125],73:[1,126],74:[1,127],75:[1,128]},{18:[1,112],19:[1,111],20:[1,113],34:[1,119],35:[1,120],51:131,52:[1,110],53:[1,114],56:[1,115],58:[1,116],59:[1,117],67:[1,118],68:[1,121],69:[1,122],70:[1,123],71:[1,124],72:[1,125],73:[1,126],74:[1,127],75:[1,128]},{5:[2,93],76:[2,93],79:[2,93],83:[2,93]},{5:[2,94],76:[2,94],79:[2,94],83:[2,94]},{76:[1,56],79:[1,57],83:[1,132]},{69:[1,133],70:[1,134],71:[1,135]},{69:[1,136],70:[1,137],71:[1,138]},{15:[1,139]},{15:[1,140]},{15:[1,141]},{15:[2,6]},{15:[2,7]},{15:[2,2]},{15:[2,3]},{15:[2,4]},{15:[2,5]},{15:[1,142]},{15:[1,143]},{15:[1,144]},{18:[1,112],19:[1,111],20:[1,113],34:[1,119],35:[1,120],51:145,52:[1,110],53:[1,114],56:[1,115],58:[1,116],59:[1,117],67:[1,118],68:[1,121],69:[1,122],70:[1,123],71:[1,124],72:[1,125],73:[1,126],74:[1,127],75:[1,128]},{18:[1,112],19:[1,111],20:[1,113],34:[1,119],35:[1,120],51:146,52:[1,110],53:[1,114],56:[1,115],58:[1,116],59:[1,117],67:[1,118],68:[1,121],69:[1,122],70:[1,123],71:[1,124],72:[1,125],73:[1,126],74:[1,127],75:[1,128]},{18:[1,112],19:[1,111],20:[1,113],34:[1,119],35:[1,120],51:147,52:[1,110],53:[1,114],56:[1,115],58:[1,116],59:[1,117],67:[1,118],68:[1,121],69:[1,122],70:[1,123],71:[1,124],72:[1,125],73:[1,126],74:[1,127],75:[1,128]},{18:[1,112],19:[1,111],20:[1,113],34:[1,119],35:[1,120],51:148,52:[1,110],53:[1,114],56:[1,115],58:[1,116],59:[1,117],67:[1,118],68:[1,121],69:[1,122],70:[1,123],71:[1,124],72:[1,125],73:[1,126],74:[1,127],75:[1,128]},{18:[1,112],19:[1,111],20:[1,113],34:[1,119],35:[1,120],51:149,52:[1,110],53:[1,114],56:[1,115],58:[1,116],59:[1,117],67:[1,118],68:[1,121],69:[1,122],70:[1,123],71:[1,124],72:[1,125],73:[1,126],74:[1,127],75:[1,128]},{18:[1,112],19:[1,111],20:[1,113],34:[1,119],35:[1,120],51:150,52:[1,110],53:[1,114],56:[1,115],58:[1,116],59:[1,117],67:[1,118],68:[1,121],69:[1,122],70:[1,123],71:[1,124],72:[1,125],73:[1,126],74:[1,127],75:[1,128]},{13:151,14:[1,152],15:[1,153]},{13:154,14:[1,152],15:[1,153]},{13:155,14:[1,152],15:[1,153]},{13:156,14:[1,152],15:[1,153]},{13:157,14:[1,152],15:[1,153]},{13:158,14:[1,152],15:[1,153]},{13:159,14:[1,152],15:[1,153]},{13:160,14:[1,152],15:[1,153]},{18:[1,112],19:[1,111],20:[1,113],34:[1,119],35:[1,120],51:161,52:[1,110],53:[1,114],56:[1,115],58:[1,116],59:[1,117],67:[1,118],68:[1,121],69:[1,122],70:[1,123],71:[1,124],72:[1,125],73:[1,126],74:[1,127],75:[1,128]},{18:[1,112],19:[1,111],20:[1,113],34:[1,119],35:[1,120],51:162,52:[1,110],53:[1,114],56:[1,115],58:[1,116],59:[1,117],67:[1,118],68:[1,121],69:[1,122],70:[1,123],71:[1,124],72:[1,125],73:[1,126],74:[1,127],75:[1,128]},{18:[1,112],19:[1,111],20:[1,113],34:[1,119],35:[1,120],51:163,52:[1,110],53:[1,114],56:[1,115],58:[1,116],59:[1,117],67:[1,118],68:[1,121],69:[1,122],70:[1,123],71:[1,124],72:[1,125],73:[1,126],74:[1,127],75:[1,128]},{18:[1,112],19:[1,111],20:[1,113],34:[1,119],35:[1,120],51:164,52:[1,110],53:[1,114],56:[1,115],58:[1,116],59:[1,117],67:[1,118],68:[1,121],69:[1,122],70:[1,123],71:[1,124],72:[1,125],73:[1,126],74:[1,127],75:[1,128]},{18:[1,112],19:[1,111],20:[1,113],34:[1,119],35:[1,120],51:165,52:[1,110],53:[1,114],56:[1,115],58:[1,116],59:[1,117],67:[1,118],68:[1,121],69:[1,122],70:[1,123],71:[1,124],72:[1,125],73:[1,126],74:[1,127],75:[1,128]},{18:[1,112],19:[1,111],20:[1,113],34:[1,119],35:[1,120],51:166,52:[1,110],53:[1,114],56:[1,115],58:[1,116],59:[1,117],67:[1,118],68:[1,121],69:[1,122],70:[1,123],71:[1,124],72:[1,125],73:[1,126],74:[1,127],75:[1,128]},{18:[1,112],19:[1,111],20:[1,113],34:[1,119],35:[1,120],51:167,52:[1,110],53:[1,114],56:[1,115],58:[1,116],59:[1,117],67:[1,118],68:[1,121],69:[1,122],70:[1,123],71:[1,124],72:[1,125],73:[1,126],74:[1,127],75:[1,128]},{18:[1,112],19:[1,111],20:[1,113],34:[1,119],35:[1,120],51:168,52:[1,110],53:[1,114],56:[1,115],58:[1,116],59:[1,117],67:[1,118],68:[1,121],69:[1,122],70:[1,123],71:[1,124],72:[1,125],73:[1,126],74:[1,127],75:[1,128]},{18:[1,112],19:[1,111],20:[1,113],34:[1,119],35:[1,120],51:169,52:[1,110],53:[1,114],56:[1,115],58:[1,116],59:[1,117],67:[1,118],68:[1,121],69:[1,122],70:[1,123],71:[1,124],72:[1,125],73:[1,126],74:[1,127],75:[1,128]},{18:[1,112],19:[1,111],20:[1,113],34:[1,119],35:[1,120],51:170,52:[1,110],53:[1,114],56:[1,115],58:[1,116],59:[1,117],67:[1,118],68:[1,121],69:[1,122],70:[1,123],71:[1,124],72:[1,125],73:[1,126],74:[1,127],75:[1,128]},{18:[1,112],19:[1,111],20:[1,113],34:[1,119],35:[1,120],51:171,52:[1,110],53:[1,114],56:[1,115],58:[1,116],59:[1,117],67:[1,118],68:[1,121],69:[1,122],70:[1,123],71:[1,124],72:[1,125],73:[1,126],74:[1,127],75:[1,128]},{18:[1,112],19:[1,111],20:[1,113],34:[1,119],35:[1,120],51:172,52:[1,110],53:[1,114],56:[1,115],58:[1,116],59:[1,117],67:[1,118],68:[1,121],69:[1,122],70:[1,123],71:[1,124],72:[1,125],73:[1,126],74:[1,127],75:[1,128]},{18:[1,112],19:[1,111],20:[1,113],34:[1,119],35:[1,120],51:173,52:[1,110],53:[1,114],56:[1,115],58:[1,116],59:[1,117],67:[1,118],68:[1,121],69:[1,122],70:[1,123],71:[1,124],72:[1,125],73:[1,126],74:[1,127],75:[1,128]},{18:[1,112],19:[1,111],20:[1,113],34:[1,119],35:[1,120],51:174,52:[1,110],53:[1,114],56:[1,115],58:[1,116],59:[1,117],67:[1,118],68:[1,121],69:[1,122],70:[1,123],71:[1,124],72:[1,125],73:[1,126],74:[1,127],75:[1,128]},{5:[2,87],76:[2,87],79:[2,87],83:[2,87]},{5:[2,92],76:[1,56],79:[2,92],83:[2,92]},{5:[2,88],76:[2,88],79:[2,88],83:[2,88]},{5:[2,41],76:[2,41],79:[2,41],83:[2,41]},{5:[2,42],76:[2,42],79:[2,42],83:[2,42]},{5:[2,43],76:[2,43],79:[2,43],83:[2,43]},{5:[2,44],76:[2,44],79:[2,44],83:[2,44]},{5:[2,45],54:[1,175],55:[1,176],76:[2,45],79:[2,45],83:[2,45]},{5:[2,48],54:[1,177],55:[1,178],57:[1,179],76:[2,48],79:[2,48],83:[2,48]},{5:[2,52],54:[1,180],55:[1,181],57:[1,182],76:[2,52],79:[2,52],83:[2,52]},{60:[1,183],63:[1,184],64:[1,185],65:[1,187],66:[1,186]},{5:[2,64],76:[2,64],79:[2,64],83:[2,64]},{5:[2,65],76:[2,65],79:[2,65],83:[2,65]},{5:[2,66],65:[1,188],76:[2,66],79:[2,66],83:[2,66]},{5:[2,68],54:[1,189],55:[1,190],76:[2,68],79:[2,68],83:[2,68]},{5:[2,73],76:[2,73],79:[2,73],83:[2,73]},{5:[2,74],76:[2,74],79:[2,74],83:[2,74]},{5:[2,75],76:[2,75],79:[2,75],83:[2,75]},{5:[2,76],54:[1,191],55:[1,192],57:[1,193],76:[2,76],79:[2,76],83:[2,76]},{5:[2,81],65:[1,194],76:[2,81],79:[2,81],83:[2,81]},{5:[2,83],65:[1,195],76:[2,83],79:[2,83],83:[2,83]},{5:[2,85],65:[1,196],76:[2,85],79:[2,85],83:[2,85]},{5:[2,89],76:[2,89],79:[2,89],83:[2,89]},{5:[2,90],76:[2,90],79:[2,90],83:[2,90]},{5:[2,91],76:[2,91],79:[2,91],83:[2,91]},{5:[2,95],76:[2,95],79:[2,95],83:[2,95]},{5:[2,96],76:[2,96],79:[2,96],83:[2,96]},{5:[2,97],76:[2,97],79:[2,97],83:[2,97]},{5:[2,98],76:[2,98],79:[2,98],83:[2,98]},{5:[2,99],76:[2,99],79:[2,99],83:[2,99]},{5:[2,100],76:[2,100],79:[2,100],83:[2,100]},{5:[2,101],76:[2,101],79:[2,101],83:[2,101]},{5:[2,102],76:[2,102],79:[2,102],83:[2,102]},{5:[2,103],76:[2,103],79:[2,103],83:[2,103]},{5:[2,104],76:[2,104],79:[2,104],83:[2,104]},{5:[2,105],76:[2,105],79:[2,105],83:[2,105]},{5:[2,112],76:[2,112],79:[2,112],83:[2,112]},{5:[2,113],76:[2,113],79:[2,113],83:[2,113]},{5:[2,106],76:[2,106],79:[2,106],83:[2,106]},{5:[2,107],76:[2,107],79:[2,107],83:[2,107]},{5:[2,108],76:[2,108],79:[2,108],83:[2,108]},{5:[2,109],76:[2,109],79:[2,109],83:[2,109]},{5:[2,110],76:[2,110],79:[2,110],83:[2,110]},{5:[2,111],76:[2,111],79:[2,111],83:[2,111]},{5:[2,114],76:[2,114],79:[2,114],83:[2,114]},{5:[2,8],76:[2,8],79:[2,8],83:[2,8]},{5:[2,9],76:[2,9],79:[2,9],83:[2,9]},{5:[2,115],76:[2,115],79:[2,115],83:[2,115]},{5:[2,116],76:[2,116],79:[2,116],83:[2,116]},{5:[2,117],76:[2,117],79:[2,117],83:[2,117]},{5:[2,118],76:[2,118],79:[2,118],83:[2,118]},{5:[2,119],76:[2,119],79:[2,119],83:[2,119]},{5:[2,120],76:[2,120],79:[2,120],83:[2,120]},{5:[2,121],76:[2,121],79:[2,121],83:[2,121]},{5:[2,122],76:[2,122],79:[2,122],83:[2,122]},{5:[2,123],76:[2,123],79:[2,123],83:[2,123]},{5:[2,124],76:[2,124],79:[2,124],83:[2,124]},{5:[2,128],76:[2,128],79:[2,128],83:[2,128]},{5:[2,125],76:[2,125],79:[2,125],83:[2,125]},{5:[2,129],76:[2,129],79:[2,129],83:[2,129]},{5:[2,126],76:[2,126],79:[2,126],83:[2,126]},{5:[2,130],76:[2,130],79:[2,130],83:[2,130]},{5:[2,127],76:[2,127],79:[2,127],83:[2,127]},{5:[2,131],76:[2,131],79:[2,131],83:[2,131]},{5:[2,132],76:[2,132],79:[2,132],83:[2,132]},{5:[2,133],76:[2,133],79:[2,133],83:[2,133]},{5:[2,134],76:[2,134],79:[2,134],83:[2,134]},{5:[2,135],76:[2,135],79:[2,135],83:[2,135]},{5:[2,46],76:[2,46],79:[2,46],83:[2,46]},{5:[2,47],76:[2,47],79:[2,47],83:[2,47]},{5:[2,49],76:[2,49],79:[2,49],83:[2,49]},{5:[2,50],76:[2,50],79:[2,50],83:[2,50]},{5:[2,51],76:[2,51],79:[2,51],83:[2,51]},{5:[2,53],76:[2,53],79:[2,53],83:[2,53]},{5:[2,54],76:[2,54],79:[2,54],83:[2,54]},{5:[2,55],76:[2,55],79:[2,55],83:[2,55]},{61:[1,197],62:[1,198]},{61:[1,199],62:[1,200]},{5:[2,60],65:[1,201],76:[2,60],79:[2,60],83:[2,60]},{5:[2,62],76:[2,62],79:[2,62],83:[2,62]},{5:[2,63],76:[2,63],79:[2,63],83:[2,63]},{5:[2,67],76:[2,67],79:[2,67],83:[2,67]},{5:[2,69],65:[1,202],76:[2,69],79:[2,69],83:[2,69]},{5:[2,71],65:[1,203],76:[2,71],79:[2,71],83:[2,71]},{5:[2,77],76:[2,77],79:[2,77],83:[2,77]},{5:[2,78],76:[2,78],79:[2,78],83:[2,78]},{5:[2,79],65:[1,204],76:[2,79],79:[2,79],83:[2,79]},{5:[2,82],76:[2,82],79:[2,82],83:[2,82]},{5:[2,84],76:[2,84],79:[2,84],83:[2,84]},{5:[2,86],76:[2,86],79:[2,86],83:[2,86]},{5:[2,56],76:[2,56],79:[2,56],83:[2,56]},{5:[2,57],76:[2,57],79:[2,57],83:[2,57]},{5:[2,58],76:[2,58],79:[2,58],83:[2,58]},{5:[2,59],76:[2,59],79:[2,59],83:[2,59]},{5:[2,61],76:[2,61],79:[2,61],83:[2,61]},{5:[2,70],76:[2,70],79:[2,70],83:[2,70]},{5:[2,72],76:[2,72],79:[2,72],83:[2,72]},{5:[2,80],76:[2,80],79:[2,80],83:[2,80]}],
defaultActions: {55:[2,1],70:[2,6],71:[2,7],72:[2,2],73:[2,3],74:[2,4],75:[2,5]},
parseError: function parseError(str, hash) {
    throw new Error(str);
},
parse: function parse(input) {
    var self = this, stack = [0], vstack = [null], lstack = [], table = this.table, yytext = "", yylineno = 0, yyleng = 0, recovering = 0, TERROR = 2, EOF = 1;
    this.lexer.setInput(input);
    this.lexer.yy = this.yy;
    this.yy.lexer = this.lexer;
    this.yy.parser = this;
    if (typeof this.lexer.yylloc == "undefined")
        this.lexer.yylloc = {};
    var yyloc = this.lexer.yylloc;
    lstack.push(yyloc);
    var ranges = this.lexer.options && this.lexer.options.ranges;
    if (typeof this.yy.parseError === "function")
        this.parseError = this.yy.parseError;
    function popStack(n) {
        stack.length = stack.length - 2 * n;
        vstack.length = vstack.length - n;
        lstack.length = lstack.length - n;
    }
    function lex() {
        var token;
        token = self.lexer.lex() || 1;
        if (typeof token !== "number") {
            token = self.symbols_[token] || token;
        }
        return token;
    }
    var symbol, preErrorSymbol, state, action, a, r, yyval = {}, p, len, newState, expected;
    while (true) {
        state = stack[stack.length - 1];
        if (this.defaultActions[state]) {
            action = this.defaultActions[state];
        } else {
            if (symbol === null || typeof symbol == "undefined") {
                symbol = lex();
            }
            action = table[state] && table[state][symbol];
        }
        if (typeof action === "undefined" || !action.length || !action[0]) {
            var errStr = "";
            if (!recovering) {
                expected = [];
                for (p in table[state])
                    if (this.terminals_[p] && p > 2) {
                        expected.push("'" + this.terminals_[p] + "'");
                    }
                if (this.lexer.showPosition) {
                    errStr = "Parse error on line " + (yylineno + 1) + ":\n" + this.lexer.showPosition() + "\nExpecting " + expected.join(", ") + ", got '" + (this.terminals_[symbol] || symbol) + "'";
                } else {
                    errStr = "Parse error on line " + (yylineno + 1) + ": Unexpected " + (symbol == 1?"end of input":"'" + (this.terminals_[symbol] || symbol) + "'");
                }
                this.parseError(errStr, {text: this.lexer.match, token: this.terminals_[symbol] || symbol, line: this.lexer.yylineno, loc: yyloc, expected: expected});
            }
        }
        if (action[0] instanceof Array && action.length > 1) {
            throw new Error("Parse Error: multiple actions possible at state: " + state + ", token: " + symbol);
        }
        switch (action[0]) {
        case 1:
            stack.push(symbol);
            vstack.push(this.lexer.yytext);
            lstack.push(this.lexer.yylloc);
            stack.push(action[1]);
            symbol = null;
            if (!preErrorSymbol) {
                yyleng = this.lexer.yyleng;
                yytext = this.lexer.yytext;
                yylineno = this.lexer.yylineno;
                yyloc = this.lexer.yylloc;
                if (recovering > 0)
                    recovering--;
            } else {
                symbol = preErrorSymbol;
                preErrorSymbol = null;
            }
            break;
        case 2:
            len = this.productions_[action[1]][1];
            yyval.$ = vstack[vstack.length - len];
            yyval._$ = {first_line: lstack[lstack.length - (len || 1)].first_line, last_line: lstack[lstack.length - 1].last_line, first_column: lstack[lstack.length - (len || 1)].first_column, last_column: lstack[lstack.length - 1].last_column};
            if (ranges) {
                yyval._$.range = [lstack[lstack.length - (len || 1)].range[0], lstack[lstack.length - 1].range[1]];
            }
            r = this.performAction.call(yyval, yytext, yyleng, yylineno, this.yy, action[1], vstack, lstack);
            if (typeof r !== "undefined") {
                return r;
            }
            if (len) {
                stack = stack.slice(0, -1 * len * 2);
                vstack = vstack.slice(0, -1 * len);
                lstack = lstack.slice(0, -1 * len);
            }
            stack.push(this.productions_[action[1]][0]);
            vstack.push(yyval.$);
            lstack.push(yyval._$);
            newState = table[stack[stack.length - 2]][stack[stack.length - 1]];
            stack.push(newState);
            break;
        case 3:
            return true;
        }
    }
    return true;
}
};

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
        t1.and.push({term: {a1: ip1>>>0}});
        t2.and.push({term: {a2: ip1>>>0}});
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
  }
}

function stripQuotes (str) {
  if (str[0] === "\"") {
    str =  str.substring(1, str.length-1);
  }
  return str;
}
/* Jison generated lexer */
var lexer = (function(){
var lexer = ({EOF:1,
parseError:function parseError(str, hash) {
        if (this.yy.parser) {
            this.yy.parser.parseError(str, hash);
        } else {
            throw new Error(str);
        }
    },
setInput:function (input) {
        this._input = input;
        this._more = this._less = this.done = false;
        this.yylineno = this.yyleng = 0;
        this.yytext = this.matched = this.match = '';
        this.conditionStack = ['INITIAL'];
        this.yylloc = {first_line:1,first_column:0,last_line:1,last_column:0};
        if (this.options.ranges) this.yylloc.range = [0,0];
        this.offset = 0;
        return this;
    },
input:function () {
        var ch = this._input[0];
        this.yytext += ch;
        this.yyleng++;
        this.offset++;
        this.match += ch;
        this.matched += ch;
        var lines = ch.match(/(?:\r\n?|\n).*/g);
        if (lines) {
            this.yylineno++;
            this.yylloc.last_line++;
        } else {
            this.yylloc.last_column++;
        }
        if (this.options.ranges) this.yylloc.range[1]++;

        this._input = this._input.slice(1);
        return ch;
    },
unput:function (ch) {
        var len = ch.length;
        var lines = ch.split(/(?:\r\n?|\n)/g);

        this._input = ch + this._input;
        this.yytext = this.yytext.substr(0, this.yytext.length-len-1);
        //this.yyleng -= len;
        this.offset -= len;
        var oldLines = this.match.split(/(?:\r\n?|\n)/g);
        this.match = this.match.substr(0, this.match.length-1);
        this.matched = this.matched.substr(0, this.matched.length-1);

        if (lines.length-1) this.yylineno -= lines.length-1;
        var r = this.yylloc.range;

        this.yylloc = {first_line: this.yylloc.first_line,
          last_line: this.yylineno+1,
          first_column: this.yylloc.first_column,
          last_column: lines ?
              (lines.length === oldLines.length ? this.yylloc.first_column : 0) + oldLines[oldLines.length - lines.length].length - lines[0].length:
              this.yylloc.first_column - len
          };

        if (this.options.ranges) {
            this.yylloc.range = [r[0], r[0] + this.yyleng - len];
        }
        return this;
    },
more:function () {
        this._more = true;
        return this;
    },
less:function (n) {
        this.unput(this.match.slice(n));
    },
pastInput:function () {
        var past = this.matched.substr(0, this.matched.length - this.match.length);
        return (past.length > 20 ? '...':'') + past.substr(-20).replace(/\n/g, "");
    },
upcomingInput:function () {
        var next = this.match;
        if (next.length < 20) {
            next += this._input.substr(0, 20-next.length);
        }
        return (next.substr(0,20)+(next.length > 20 ? '...':'')).replace(/\n/g, "");
    },
showPosition:function () {
        var pre = this.pastInput();
        var c = new Array(pre.length + 1).join("-");
        return pre + this.upcomingInput() + "\n" + c+"^";
    },
next:function () {
        if (this.done) {
            return this.EOF;
        }
        if (!this._input) this.done = true;

        var token,
            match,
            tempMatch,
            index,
            col,
            lines;
        if (!this._more) {
            this.yytext = '';
            this.match = '';
        }
        var rules = this._currentRules();
        for (var i=0;i < rules.length; i++) {
            tempMatch = this._input.match(this.rules[rules[i]]);
            if (tempMatch && (!match || tempMatch[0].length > match[0].length)) {
                match = tempMatch;
                index = i;
                if (!this.options.flex) break;
            }
        }
        if (match) {
            lines = match[0].match(/(?:\r\n?|\n).*/g);
            if (lines) this.yylineno += lines.length;
            this.yylloc = {first_line: this.yylloc.last_line,
                           last_line: this.yylineno+1,
                           first_column: this.yylloc.last_column,
                           last_column: lines ? lines[lines.length-1].length-lines[lines.length-1].match(/\r?\n?/)[0].length : this.yylloc.last_column + match[0].length};
            this.yytext += match[0];
            this.match += match[0];
            this.matches = match;
            this.yyleng = this.yytext.length;
            if (this.options.ranges) {
                this.yylloc.range = [this.offset, this.offset += this.yyleng];
            }
            this._more = false;
            this._input = this._input.slice(match[0].length);
            this.matched += match[0];
            token = this.performAction.call(this, this.yy, this, rules[index],this.conditionStack[this.conditionStack.length-1]);
            if (this.done && this._input) this.done = false;
            if (token) return token;
            else return;
        }
        if (this._input === "") {
            return this.EOF;
        } else {
            return this.parseError('Lexical error on line '+(this.yylineno+1)+'. Unrecognized text.\n'+this.showPosition(),
                    {text: "", token: null, line: this.yylineno});
        }
    },
lex:function lex() {
        var r = this.next();
        if (typeof r !== 'undefined') {
            return r;
        } else {
            return this.lex();
        }
    },
begin:function begin(condition) {
        this.conditionStack.push(condition);
    },
popState:function popState() {
        return this.conditionStack.pop();
    },
_currentRules:function _currentRules() {
        return this.conditions[this.conditionStack[this.conditionStack.length-1]].rules;
    },
topState:function () {
        return this.conditionStack[this.conditionStack.length-2];
    },
pushState:function begin(condition) {
        this.begin(condition);
    }});
lexer.options = {"flex":true};
lexer.performAction = function anonymous(yy,yy_,$avoiding_name_collisions,YY_START) {

var YYSTATE=YY_START
switch($avoiding_name_collisions) {
case 0:/* skip whitespace */
break;
case 1:return 15
break;
case 2:return 14
break;
case 3:return 18
break;
case 4:return 17
break;
case 5:return 19
break;
case 6:return 20
break;
case 7:return 21
break;
case 8:return 22
break;
case 9:return 53
break;
case 10:return 34
break;
case 11:return 42
break;
case 12:return 43
break;
case 13:return 44
break;
case 14:return 56
break;
case 15:return 46
break;
case 16:return 47
break;
case 17:return 48
break;
case 18:return 58
break;
case 19:return "ip.src"
break;
case 20:return "ip.dst"
break;
case 21:return "ip.xff"
break;
case 22:return "ip.xff.cnt"
break;
case 23:return "ip"
break;
case 24:return "cert.issuer.cn"
break;
case 25:return "cert.issuer.on"
break;
case 26:return "cert.subject.cn"
break;
case 27:return "cert.subject.on"
break;
case 28:return "cert.alt"
break;
case 29:return "cert.alt.cnt"
break;
case 30:return "cert.serial"
break;
case 31:return "cert.cnt"
break;
case 32:return "uri"
break;
case 33:return "uri.cnt"
break;
case 34:return "ua"
break;
case 35:return "ua.cnt"
break;
case 36:return "user"
break;
case 37:return "user.cnt"
break;
case 38:return "icmp"
break;
case 39:return "tcp"
break;
case 40:return "udp"
break;
case 41:return "host"
break;
case 42:return "host.cnt"
break;
case 43:return "oldheader"
break;
case 44:return "header"
break;
case 45:return "header.src"
break;
case 46:return "header.src.cnt"
break;
case 47:return "header.dst"
break;
case 48:return "header.dst.cnt"
break;
case 49:return 75
break;
case 50:return 31
break;
case 51:return 52
break;
case 52:return 67
break;
case 53:return 8
break;
case 54:return 7
break;
case 55:return 11
break;
case 56:return 10
break;
case 57:return 78
break;
case 58:return 77
break;
case 59:return 77
break;
case 60:return 79
break;
case 61:return 79
break;
case 62:return 76
break;
case 63:return 76
break;
case 64:return 82
break;
case 65:return 83
break;
case 66:return 80
break;
case 67:return 5
break;
case 68:return 'INVALID'
break;
case 69:console.log(yy_.yytext);
break;
}
};
lexer.rules = [/^(?:\s+)/,/^(?:[0-9]+\b)/,/^(?:([0-9]{1,3})?(\.[0-9]{1,3})?(\.[0-9]{1,3})?(\.[0-9]{1,3})?(\/[0-9]{1,2})?(:[0-9]{1,5})?\b)/,/^(?:bytes)/,/^(?:databytes)/,/^(?:packets)/,/^(?:protocol)/,/^(?:port\.src)/,/^(?:port\.dst)/,/^(?:port)/,/^(?:node)/,/^(?:country\.src)/,/^(?:country\.dst)/,/^(?:country\.xff)/,/^(?:country)/,/^(?:asn\.src)/,/^(?:asn\.dst)/,/^(?:asn\.xff)/,/^(?:asn)/,/^(?:ip\.src)/,/^(?:ip\.dst)/,/^(?:ip\.xff)/,/^(?:ip\.xff\.cnt)/,/^(?:ip)/,/^(?:cert\.issuer\.cn)/,/^(?:cert\.issuer\.on)/,/^(?:cert\.subject\.cn)/,/^(?:cert\.subject\.on)/,/^(?:cert\.alt)/,/^(?:cert\.alt\.cnt)/,/^(?:cert\.serial)/,/^(?:cert\.cnt)/,/^(?:uri)/,/^(?:uri\.cnt)/,/^(?:ua)/,/^(?:ua\.cnt)/,/^(?:user)/,/^(?:user\.cnt)/,/^(?:icmp)/,/^(?:tcp)/,/^(?:udp)/,/^(?:host)/,/^(?:host\.cnt)/,/^(?:oldheader)/,/^(?:header)/,/^(?:header\.src)/,/^(?:header\.src\.cnt)/,/^(?:header\.dst)/,/^(?:header\.dst\.cnt)/,/^(?:tags)/,/^(?:tags\.cnt)/,/^(?:[/\w*._:-]+)/,/^(?:"[^"]+")/,/^(?:<=)/,/^(?:<)/,/^(?:>=)/,/^(?:>)/,/^(?:!=)/,/^(?:==)/,/^(?:=)/,/^(?:\|\|)/,/^(?:\|)/,/^(?:&&)/,/^(?:&)/,/^(?:\()/,/^(?:\))/,/^(?:!)/,/^(?:$)/,/^(?:.)/,/^(?:.)/];
lexer.conditions = {"INITIAL":{"rules":[0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64,65,66,67,68,69],"inclusive":true}};
return lexer;})()
parser.lexer = lexer;function Parser () { this.yy = {}; }Parser.prototype = parser;parser.Parser = Parser;
return new Parser;
})();
if (typeof require !== 'undefined' && typeof exports !== 'undefined') {
exports.parser = molochparser;
exports.Parser = molochparser.Parser;
exports.parse = function () { return molochparser.parse.apply(molochparser, arguments); }
exports.main = function commonjsMain(args) {
    if (!args[1])
        throw new Error('Usage: '+args[0]+' FILE');
    var source, cwd;
    if (typeof process !== 'undefined') {
        source = require('fs').readFileSync(require('path').resolve(args[1]), "utf8");
    } else {
        source = require("file").path(require("file").cwd()).join(args[1]).read({charset: "utf-8"});
    }
    return exports.parser.parse(source);
}
if (typeof module !== 'undefined' && require.main === module) {
  exports.main(typeof process !== 'undefined' ? process.argv.slice(1) : require("system").args);
}
}