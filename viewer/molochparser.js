/* Jison generated parser */
var molochparser = (function(){
var parser = {trace: function trace() { },
yy: {},
symbols_: {"error":2,"expressions":3,"e":4,"EOF":5,"LTA":6,"lt":7,"lte":8,"GTA":9,"gt":10,"gte":11,"GTLT":12,"IPNUM":13,"IPMATCH":14,"NUMBER":15,"RANGEFIELD":16,"databytes":17,"bytes":18,"packets":19,"protocol":20,"port.src":21,"port.dst":22,"uri.cnt":23,"cert.cnt":24,"ip.xff.cnt":25,"ua.cnt":26,"host.cnt":27,"header.src.cnt":28,"header.dst.cnt":29,"tags.cnt":30,"cert.alt.cnt":31,"TERMFIELD":32,"node":33,"host":34,"cert.subject.cn":35,"cert.issuer.cn":36,"cert.serial":37,"cert.alt":38,"UPTERMFIELD":39,"country.src":40,"country.dst":41,"country.xff":42,"TEXTFIELD":43,"asn.src":44,"asn.dst":45,"asn.xff":46,"cert.subject.on":47,"cert.issuer.on":48,"STR":49,"ID":50,"port":51,"src":52,"dst":53,"country":54,"xff":55,"asn":56,"cert":57,"issuer":58,"cn":59,"on":60,"subject":61,"alt":62,"cnt":63,"serial":64,"QUOTEDSTR":65,"header":66,"icmp":67,"tcp":68,"udp":69,"ip":70,"uri":71,"ua":72,"tags":73,"&&":74,"==":75,"!=":76,"||":77,"!":78,"-":79,"(":80,")":81,"ip.src":82,"ip.dst":83,"ip.xff":84,"oldheader":85,"header.src":86,"header.dst":87,"$accept":0,"$end":1},
terminals_: {2:"error",5:"EOF",7:"lt",8:"lte",10:"gt",11:"gte",14:"IPMATCH",15:"NUMBER",17:"databytes",18:"bytes",19:"packets",20:"protocol",21:"port.src",22:"port.dst",23:"uri.cnt",24:"cert.cnt",25:"ip.xff.cnt",26:"ua.cnt",27:"host.cnt",28:"header.src.cnt",29:"header.dst.cnt",30:"tags.cnt",31:"cert.alt.cnt",33:"node",34:"host",35:"cert.subject.cn",36:"cert.issuer.cn",37:"cert.serial",38:"cert.alt",40:"country.src",41:"country.dst",42:"country.xff",44:"asn.src",45:"asn.dst",46:"asn.xff",47:"cert.subject.on",48:"cert.issuer.on",50:"ID",51:"port",52:"src",53:"dst",54:"country",55:"xff",56:"asn",57:"cert",58:"issuer",59:"cn",60:"on",61:"subject",62:"alt",63:"cnt",64:"serial",65:"QUOTEDSTR",66:"header",67:"icmp",68:"tcp",69:"udp",70:"ip",71:"uri",72:"ua",73:"tags",74:"&&",75:"==",76:"!=",77:"||",78:"!",79:"-",80:"(",81:")",82:"ip.src",83:"ip.dst",84:"ip.xff",85:"oldheader",86:"header.src",87:"header.dst"},
productions_: [0,[3,2],[6,1],[6,1],[9,1],[9,1],[12,1],[12,1],[13,1],[13,1],[16,1],[16,1],[16,1],[16,1],[16,1],[16,1],[16,1],[16,1],[16,1],[16,1],[16,1],[16,1],[16,1],[16,1],[16,1],[32,1],[32,1],[32,1],[32,1],[32,1],[32,1],[39,1],[39,1],[39,1],[43,1],[43,1],[43,1],[43,1],[43,1],[49,1],[49,1],[49,1],[49,1],[49,1],[49,2],[49,2],[49,1],[49,2],[49,2],[49,2],[49,1],[49,2],[49,2],[49,2],[49,3],[49,3],[49,3],[49,3],[49,2],[49,3],[49,2],[49,2],[49,1],[49,1],[49,1],[49,2],[49,1],[49,2],[49,3],[49,2],[49,3],[49,1],[49,1],[49,1],[49,1],[49,2],[49,2],[49,2],[49,3],[49,1],[49,2],[49,1],[49,2],[49,1],[49,2],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,2],[4,2],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3]],
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
case 20:this.$ = 'hocnt'
break;
case 21:this.$ = 'hh1cnt'
break;
case 22:this.$ = 'hh2cnt'
break;
case 23:this.$ = 'tacnt'
break;
case 24:this.$ = 'tls.altcnt'
break;
case 25:this.$ = 'no'
break;
case 26:this.$ = 'ho'
break;
case 27:this.$ = 'tls.sCn'
break;
case 28:this.$ = 'tls.iCn'
break;
case 29:this.$ = 'tls.sn'
break;
case 30:this.$ = 'tls.alt'
break;
case 31:this.$ = 'g1'
break;
case 32:this.$ = 'g2'
break;
case 33:this.$ = 'gxff'
break;
case 34:this.$ = 'as1'
break;
case 35:this.$ = 'as2'
break;
case 36:this.$ = 'asxff'
break;
case 37:this.$ = 'tls.sOn'
break;
case 38:this.$ = 'tls.iOn'
break;
case 85:this.$ = {and: [$$[$0-2], $$[$0]]};
break;
case 86:this.$ = {query: {text: {us: {query: $$[$0], type: "phrase", operator: "and"}}}};
break;
case 87:this.$ = {not: {query: {text: {us: {query: $$[$0], type: "phrase", operator: "and"}}}}};
break;
case 88:this.$ = {query: {text: {ua: {query: $$[$0], type: "phrase", operator: "and"}}}};
break;
case 89:this.$ = {not: {query: {text: {ua: {query: $$[$0], type: "phrase", operator: "and"}}}}};
break;
case 90:this.$ = {or: [$$[$0-2], $$[$0]]};
break;
case 91:this.$ = {not: $$[$0]};
break;
case 92:this.$ = -$$[$0];
break;
case 93:this.$ = $$[$0-1];
break;
case 94:this.$ = {term: {pr: 1}};
break;
case 95:this.$ = {term: {pr: 6}};
break;
case 96:this.$ = {term: {pr: 17}};
break;
case 97:this.$ = {not: {term: {pr: 1}}};
break;
case 98:this.$ = {not: {term: {pr: 6}}};
break;
case 99:this.$ = {not: {term: {pr: 17}}};
break;
case 100:this.$ = {range: {}};
         this.$.range[$$[$0-2]] = {};
         this.$.range[$$[$0-2]][$$[$0-1]] = $$[$0];
break;
case 101:this.$ = {term: {}};
         this.$.term[$$[$0-2]] = $$[$0];
break;
case 102:this.$ = {not: {term: {}}};
         this.$.not.term[$$[$0-2]] = $$[$0];
break;
case 103:this.$ = {or: [{range: {p1: {}}}, {range: {p2: {}}}]};
         this.$.or[0].range.p1[$$[$0-1]] = $$[$0];
         this.$.or[1].range.p2[$$[$0-1]] = $$[$0];
break;
case 104: var str = stripQuotes($$[$0]).toLowerCase();
          if (str.indexOf("*") !== -1) {
            this.$ = {not: {query: {wildcard: {}}}};
            this.$.not.query.wildcard[$$[$0-2]] = str;
          } else {
            this.$ = {not: {term: {}}};
            this.$.not.term[$$[$0-2]] = str;
          }
        
break;
case 105: var str = stripQuotes($$[$0]).toLowerCase();
          if (str.indexOf("*") !== -1) {
            this.$ = {query: {wildcard: {}}};
            this.$.query.wildcard[$$[$0-2]] = str;
          } else {
            this.$ = {term: {}};
            this.$.term[$$[$0-2]] = str;
          }
        
break;
case 106: var str = stripQuotes($$[$0]).toUpperCase();
          if (str.indexOf("*") !== -1) {
            this.$ = {not: {query: {wildcard: {}}}};
            this.$.not.query.wildcard[$$[$0-2]] = str;
          } else {
            this.$ = {not: {term: {}}};
            this.$.not.term[$$[$0-2]] = str;
          }
        
break;
case 107: var str = stripQuotes($$[$0]).toUpperCase();
          if (str.indexOf("*") !== -1) {
            this.$ = {query: {wildcard: {}}};
            this.$.query.wildcard[$$[$0-2]] = str;
          } else {
            this.$ = {term: {}};
            this.$.term[$$[$0-2]] = str;
          }
        
break;
case 108: var str = stripQuotes($$[$0]).toLowerCase();
          if (str.indexOf("*") !== -1) {
            this.$ = {not: {query: {wildcard: {}}}};
            this.$.not.query.wildcard[$$[$0-2]] = str;
          } else {
            this.$ = {not: {query: {text: {}}}};
            this.$.not.query.text[$$[$0-2]] = {query: str, type: "phrase", operator: "and"}
          }
        
break;
case 109: var str = stripQuotes($$[$0]).toLowerCase();
          if (str.indexOf("*") !== -1) {
            this.$ = {query: {wildcard: {}}};
            this.$.query.wildcard[$$[$0-2]] = str;
          } else {
            this.$ = {query: {text: {}}};
            this.$.query.text[$$[$0-2]] = {query: str, type: "phrase", operator: "and"}
          }
        
break;
case 110:this.$ = {or: [{term: {p1: $$[$0]}}, {term: {p2: $$[$0]}}]};
break;
case 111:this.$ = {not: {or: [{term: {p1: $$[$0]}}, {term: {p2: $$[$0]}}]}};
break;
case 112:this.$ = parseIpPort($$[$0],0);
break;
case 113:this.$ = {not: parseIpPort($$[$0],0)};
break;
case 114:this.$ = parseIpPort($$[$0],1);
break;
case 115:this.$ = {not: parseIpPort($$[$0],1)};
break;
case 116:this.$ = parseIpPort($$[$0],2);
break;
case 117:this.$ = {not: parseIpPort($$[$0],2)};
break;
case 118:this.$ = parseIpPort($$[$0],3);
break;
case 119:this.$ = {not: parseIpPort($$[$0],3)};
break;
case 120: var tag = stripQuotes($$[$0]);
          this.$ = {term: {ta: tag}};
        
break;
case 121: var tag = stripQuotes($$[$0]);
          this.$ = {not: {term: {ta: tag}}};
        
break;
case 122: var tag = stripQuotes($$[$0]);
          this.$ = {term: {hh: tag}};
        
break;
case 123: var tag = stripQuotes($$[$0]);
          this.$ = {or: [{term: {hh1: tag}}, {term:{hh2: tag}}]};
        
break;
case 124: var tag = stripQuotes($$[$0]);
          this.$ = {term: {hh1: tag}};
        
break;
case 125: var tag = stripQuotes($$[$0]);
          this.$ = {term: {hh2: tag}};
        
break;
case 126: var tag = stripQuotes($$[$0]);
          this.$ = {not: {term: {hh1: tag}}};
        
break;
case 127: var tag = stripQuotes($$[$0]);
          this.$ = {not: {or: [{term: {hh1: tag}}, {term:{hh2: tag}}]}};
        
break;
case 128: var tag = stripQuotes($$[$0]);
          this.$ = {not: {term: {hh1: tag}}};
        
break;
case 129: var tag = stripQuotes($$[$0]);
          this.$ = {not: {term: {hh2: tag}}};
        
break;
case 130: var str = stripQuotes($$[$0]).toUpperCase();
          if (str.indexOf("*") !== -1) {
            this.$ = {or: [{query: {wildcard: {g1: str}}}, {query: {wildcard: {g2: str}}}, {query: {wildcard: {gxff: str}}}]};
          } else {
            this.$ = {or: [{term: {g1: str}}, {term: {g2: str}}, {term: {gxff: str}}]};
          }
        
break;
case 131: var str = stripQuotes($$[$0]).toUpperCase();
          if (str.indexOf("*") !== -1) {
            this.$ = {not: {or: [{query: {wildcard: {g1: str}}}, {query: {wildcard: {g2: str}}}, {query: {wildcard: {gxff: str}}}]}};
          } else {
            this.$ = {not: {or: [{term: {g1: str}}, {term: {g2: str}}, {term: {gxff: str}}]}};
          }
        
break;
case 132: var str = stripQuotes($$[$0]).toLowerCase();
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
case 133: var str = stripQuotes($$[$0]).toLowerCase();
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
table: [{3:1,4:2,16:9,17:[1,25],18:[1,26],19:[1,27],20:[1,8],21:[1,28],22:[1,29],23:[1,30],24:[1,31],25:[1,32],26:[1,33],27:[1,34],28:[1,35],29:[1,36],30:[1,37],31:[1,38],32:11,33:[1,39],34:[1,40],35:[1,41],36:[1,42],37:[1,43],38:[1,44],39:12,40:[1,45],41:[1,46],42:[1,47],43:13,44:[1,48],45:[1,49],46:[1,50],47:[1,51],48:[1,52],51:[1,10],54:[1,23],56:[1,24],66:[1,20],70:[1,14],71:[1,3],72:[1,4],73:[1,18],78:[1,5],79:[1,6],80:[1,7],82:[1,15],83:[1,16],84:[1,17],85:[1,19],86:[1,21],87:[1,22]},{1:[3]},{5:[1,53],74:[1,54],77:[1,55]},{75:[1,56],76:[1,57]},{75:[1,58],76:[1,59]},{4:60,16:9,17:[1,25],18:[1,26],19:[1,27],20:[1,8],21:[1,28],22:[1,29],23:[1,30],24:[1,31],25:[1,32],26:[1,33],27:[1,34],28:[1,35],29:[1,36],30:[1,37],31:[1,38],32:11,33:[1,39],34:[1,40],35:[1,41],36:[1,42],37:[1,43],38:[1,44],39:12,40:[1,45],41:[1,46],42:[1,47],43:13,44:[1,48],45:[1,49],46:[1,50],47:[1,51],48:[1,52],51:[1,10],54:[1,23],56:[1,24],66:[1,20],70:[1,14],71:[1,3],72:[1,4],73:[1,18],78:[1,5],79:[1,6],80:[1,7],82:[1,15],83:[1,16],84:[1,17],85:[1,19],86:[1,21],87:[1,22]},{4:61,16:9,17:[1,25],18:[1,26],19:[1,27],20:[1,8],21:[1,28],22:[1,29],23:[1,30],24:[1,31],25:[1,32],26:[1,33],27:[1,34],28:[1,35],29:[1,36],30:[1,37],31:[1,38],32:11,33:[1,39],34:[1,40],35:[1,41],36:[1,42],37:[1,43],38:[1,44],39:12,40:[1,45],41:[1,46],42:[1,47],43:13,44:[1,48],45:[1,49],46:[1,50],47:[1,51],48:[1,52],51:[1,10],54:[1,23],56:[1,24],66:[1,20],70:[1,14],71:[1,3],72:[1,4],73:[1,18],78:[1,5],79:[1,6],80:[1,7],82:[1,15],83:[1,16],84:[1,17],85:[1,19],86:[1,21],87:[1,22]},{4:62,16:9,17:[1,25],18:[1,26],19:[1,27],20:[1,8],21:[1,28],22:[1,29],23:[1,30],24:[1,31],25:[1,32],26:[1,33],27:[1,34],28:[1,35],29:[1,36],30:[1,37],31:[1,38],32:11,33:[1,39],34:[1,40],35:[1,41],36:[1,42],37:[1,43],38:[1,44],39:12,40:[1,45],41:[1,46],42:[1,47],43:13,44:[1,48],45:[1,49],46:[1,50],47:[1,51],48:[1,52],51:[1,10],54:[1,23],56:[1,24],66:[1,20],70:[1,14],71:[1,3],72:[1,4],73:[1,18],78:[1,5],79:[1,6],80:[1,7],82:[1,15],83:[1,16],84:[1,17],85:[1,19],86:[1,21],87:[1,22]},{7:[2,13],8:[2,13],10:[2,13],11:[2,13],75:[1,63],76:[1,64]},{6:68,7:[1,70],8:[1,71],9:69,10:[1,72],11:[1,73],12:65,75:[1,66],76:[1,67]},{6:68,7:[1,70],8:[1,71],9:69,10:[1,72],11:[1,73],12:74,75:[1,75],76:[1,76]},{75:[1,78],76:[1,77]},{75:[1,80],76:[1,79]},{75:[1,82],76:[1,81]},{75:[1,83],76:[1,84]},{75:[1,85],76:[1,86]},{75:[1,87],76:[1,88]},{75:[1,89],76:[1,90]},{75:[1,91],76:[1,92]},{75:[1,93],76:[1,94]},{75:[1,95],76:[1,96]},{75:[1,97],76:[1,98]},{75:[1,99],76:[1,100]},{75:[1,101],76:[1,102]},{75:[1,103],76:[1,104]},{7:[2,10],8:[2,10],10:[2,10],11:[2,10],75:[2,10],76:[2,10]},{7:[2,11],8:[2,11],10:[2,11],11:[2,11],75:[2,11],76:[2,11]},{7:[2,12],8:[2,12],10:[2,12],11:[2,12],75:[2,12],76:[2,12]},{7:[2,14],8:[2,14],10:[2,14],11:[2,14],75:[2,14],76:[2,14]},{7:[2,15],8:[2,15],10:[2,15],11:[2,15],75:[2,15],76:[2,15]},{7:[2,16],8:[2,16],10:[2,16],11:[2,16],75:[2,16],76:[2,16]},{7:[2,17],8:[2,17],10:[2,17],11:[2,17],75:[2,17],76:[2,17]},{7:[2,18],8:[2,18],10:[2,18],11:[2,18],75:[2,18],76:[2,18]},{7:[2,19],8:[2,19],10:[2,19],11:[2,19],75:[2,19],76:[2,19]},{7:[2,20],8:[2,20],10:[2,20],11:[2,20],75:[2,20],76:[2,20]},{7:[2,21],8:[2,21],10:[2,21],11:[2,21],75:[2,21],76:[2,21]},{7:[2,22],8:[2,22],10:[2,22],11:[2,22],75:[2,22],76:[2,22]},{7:[2,23],8:[2,23],10:[2,23],11:[2,23],75:[2,23],76:[2,23]},{7:[2,24],8:[2,24],10:[2,24],11:[2,24],75:[2,24],76:[2,24]},{75:[2,25],76:[2,25]},{75:[2,26],76:[2,26]},{75:[2,27],76:[2,27]},{75:[2,28],76:[2,28]},{75:[2,29],76:[2,29]},{75:[2,30],76:[2,30]},{75:[2,31],76:[2,31]},{75:[2,32],76:[2,32]},{75:[2,33],76:[2,33]},{75:[2,34],76:[2,34]},{75:[2,35],76:[2,35]},{75:[2,36],76:[2,36]},{75:[2,37],76:[2,37]},{75:[2,38],76:[2,38]},{1:[2,1]},{4:105,16:9,17:[1,25],18:[1,26],19:[1,27],20:[1,8],21:[1,28],22:[1,29],23:[1,30],24:[1,31],25:[1,32],26:[1,33],27:[1,34],28:[1,35],29:[1,36],30:[1,37],31:[1,38],32:11,33:[1,39],34:[1,40],35:[1,41],36:[1,42],37:[1,43],38:[1,44],39:12,40:[1,45],41:[1,46],42:[1,47],43:13,44:[1,48],45:[1,49],46:[1,50],47:[1,51],48:[1,52],51:[1,10],54:[1,23],56:[1,24],66:[1,20],70:[1,14],71:[1,3],72:[1,4],73:[1,18],78:[1,5],79:[1,6],80:[1,7],82:[1,15],83:[1,16],84:[1,17],85:[1,19],86:[1,21],87:[1,22]},{4:106,16:9,17:[1,25],18:[1,26],19:[1,27],20:[1,8],21:[1,28],22:[1,29],23:[1,30],24:[1,31],25:[1,32],26:[1,33],27:[1,34],28:[1,35],29:[1,36],30:[1,37],31:[1,38],32:11,33:[1,39],34:[1,40],35:[1,41],36:[1,42],37:[1,43],38:[1,44],39:12,40:[1,45],41:[1,46],42:[1,47],43:13,44:[1,48],45:[1,49],46:[1,50],47:[1,51],48:[1,52],51:[1,10],54:[1,23],56:[1,24],66:[1,20],70:[1,14],71:[1,3],72:[1,4],73:[1,18],78:[1,5],79:[1,6],80:[1,7],82:[1,15],83:[1,16],84:[1,17],85:[1,19],86:[1,21],87:[1,22]},{18:[1,110],19:[1,109],20:[1,111],33:[1,117],34:[1,118],49:107,50:[1,108],51:[1,112],54:[1,113],56:[1,114],57:[1,115],65:[1,116],66:[1,119],67:[1,120],68:[1,121],69:[1,122],70:[1,123],71:[1,124],72:[1,125],73:[1,126]},{18:[1,110],19:[1,109],20:[1,111],33:[1,117],34:[1,118],49:127,50:[1,108],51:[1,112],54:[1,113],56:[1,114],57:[1,115],65:[1,116],66:[1,119],67:[1,120],68:[1,121],69:[1,122],70:[1,123],71:[1,124],72:[1,125],73:[1,126]},{18:[1,110],19:[1,109],20:[1,111],33:[1,117],34:[1,118],49:128,50:[1,108],51:[1,112],54:[1,113],56:[1,114],57:[1,115],65:[1,116],66:[1,119],67:[1,120],68:[1,121],69:[1,122],70:[1,123],71:[1,124],72:[1,125],73:[1,126]},{18:[1,110],19:[1,109],20:[1,111],33:[1,117],34:[1,118],49:129,50:[1,108],51:[1,112],54:[1,113],56:[1,114],57:[1,115],65:[1,116],66:[1,119],67:[1,120],68:[1,121],69:[1,122],70:[1,123],71:[1,124],72:[1,125],73:[1,126]},{5:[2,91],74:[2,91],77:[2,91],81:[2,91]},{5:[2,92],74:[2,92],77:[2,92],81:[2,92]},{74:[1,54],77:[1,55],81:[1,130]},{67:[1,131],68:[1,132],69:[1,133]},{67:[1,134],68:[1,135],69:[1,136]},{15:[1,137]},{15:[1,138]},{15:[1,139]},{15:[2,6]},{15:[2,7]},{15:[2,2]},{15:[2,3]},{15:[2,4]},{15:[2,5]},{15:[1,140]},{15:[1,141]},{15:[1,142]},{18:[1,110],19:[1,109],20:[1,111],33:[1,117],34:[1,118],49:143,50:[1,108],51:[1,112],54:[1,113],56:[1,114],57:[1,115],65:[1,116],66:[1,119],67:[1,120],68:[1,121],69:[1,122],70:[1,123],71:[1,124],72:[1,125],73:[1,126]},{18:[1,110],19:[1,109],20:[1,111],33:[1,117],34:[1,118],49:144,50:[1,108],51:[1,112],54:[1,113],56:[1,114],57:[1,115],65:[1,116],66:[1,119],67:[1,120],68:[1,121],69:[1,122],70:[1,123],71:[1,124],72:[1,125],73:[1,126]},{18:[1,110],19:[1,109],20:[1,111],33:[1,117],34:[1,118],49:145,50:[1,108],51:[1,112],54:[1,113],56:[1,114],57:[1,115],65:[1,116],66:[1,119],67:[1,120],68:[1,121],69:[1,122],70:[1,123],71:[1,124],72:[1,125],73:[1,126]},{18:[1,110],19:[1,109],20:[1,111],33:[1,117],34:[1,118],49:146,50:[1,108],51:[1,112],54:[1,113],56:[1,114],57:[1,115],65:[1,116],66:[1,119],67:[1,120],68:[1,121],69:[1,122],70:[1,123],71:[1,124],72:[1,125],73:[1,126]},{18:[1,110],19:[1,109],20:[1,111],33:[1,117],34:[1,118],49:147,50:[1,108],51:[1,112],54:[1,113],56:[1,114],57:[1,115],65:[1,116],66:[1,119],67:[1,120],68:[1,121],69:[1,122],70:[1,123],71:[1,124],72:[1,125],73:[1,126]},{18:[1,110],19:[1,109],20:[1,111],33:[1,117],34:[1,118],49:148,50:[1,108],51:[1,112],54:[1,113],56:[1,114],57:[1,115],65:[1,116],66:[1,119],67:[1,120],68:[1,121],69:[1,122],70:[1,123],71:[1,124],72:[1,125],73:[1,126]},{13:149,14:[1,150],15:[1,151]},{13:152,14:[1,150],15:[1,151]},{13:153,14:[1,150],15:[1,151]},{13:154,14:[1,150],15:[1,151]},{13:155,14:[1,150],15:[1,151]},{13:156,14:[1,150],15:[1,151]},{13:157,14:[1,150],15:[1,151]},{13:158,14:[1,150],15:[1,151]},{18:[1,110],19:[1,109],20:[1,111],33:[1,117],34:[1,118],49:159,50:[1,108],51:[1,112],54:[1,113],56:[1,114],57:[1,115],65:[1,116],66:[1,119],67:[1,120],68:[1,121],69:[1,122],70:[1,123],71:[1,124],72:[1,125],73:[1,126]},{18:[1,110],19:[1,109],20:[1,111],33:[1,117],34:[1,118],49:160,50:[1,108],51:[1,112],54:[1,113],56:[1,114],57:[1,115],65:[1,116],66:[1,119],67:[1,120],68:[1,121],69:[1,122],70:[1,123],71:[1,124],72:[1,125],73:[1,126]},{18:[1,110],19:[1,109],20:[1,111],33:[1,117],34:[1,118],49:161,50:[1,108],51:[1,112],54:[1,113],56:[1,114],57:[1,115],65:[1,116],66:[1,119],67:[1,120],68:[1,121],69:[1,122],70:[1,123],71:[1,124],72:[1,125],73:[1,126]},{18:[1,110],19:[1,109],20:[1,111],33:[1,117],34:[1,118],49:162,50:[1,108],51:[1,112],54:[1,113],56:[1,114],57:[1,115],65:[1,116],66:[1,119],67:[1,120],68:[1,121],69:[1,122],70:[1,123],71:[1,124],72:[1,125],73:[1,126]},{18:[1,110],19:[1,109],20:[1,111],33:[1,117],34:[1,118],49:163,50:[1,108],51:[1,112],54:[1,113],56:[1,114],57:[1,115],65:[1,116],66:[1,119],67:[1,120],68:[1,121],69:[1,122],70:[1,123],71:[1,124],72:[1,125],73:[1,126]},{18:[1,110],19:[1,109],20:[1,111],33:[1,117],34:[1,118],49:164,50:[1,108],51:[1,112],54:[1,113],56:[1,114],57:[1,115],65:[1,116],66:[1,119],67:[1,120],68:[1,121],69:[1,122],70:[1,123],71:[1,124],72:[1,125],73:[1,126]},{18:[1,110],19:[1,109],20:[1,111],33:[1,117],34:[1,118],49:165,50:[1,108],51:[1,112],54:[1,113],56:[1,114],57:[1,115],65:[1,116],66:[1,119],67:[1,120],68:[1,121],69:[1,122],70:[1,123],71:[1,124],72:[1,125],73:[1,126]},{18:[1,110],19:[1,109],20:[1,111],33:[1,117],34:[1,118],49:166,50:[1,108],51:[1,112],54:[1,113],56:[1,114],57:[1,115],65:[1,116],66:[1,119],67:[1,120],68:[1,121],69:[1,122],70:[1,123],71:[1,124],72:[1,125],73:[1,126]},{18:[1,110],19:[1,109],20:[1,111],33:[1,117],34:[1,118],49:167,50:[1,108],51:[1,112],54:[1,113],56:[1,114],57:[1,115],65:[1,116],66:[1,119],67:[1,120],68:[1,121],69:[1,122],70:[1,123],71:[1,124],72:[1,125],73:[1,126]},{18:[1,110],19:[1,109],20:[1,111],33:[1,117],34:[1,118],49:168,50:[1,108],51:[1,112],54:[1,113],56:[1,114],57:[1,115],65:[1,116],66:[1,119],67:[1,120],68:[1,121],69:[1,122],70:[1,123],71:[1,124],72:[1,125],73:[1,126]},{18:[1,110],19:[1,109],20:[1,111],33:[1,117],34:[1,118],49:169,50:[1,108],51:[1,112],54:[1,113],56:[1,114],57:[1,115],65:[1,116],66:[1,119],67:[1,120],68:[1,121],69:[1,122],70:[1,123],71:[1,124],72:[1,125],73:[1,126]},{18:[1,110],19:[1,109],20:[1,111],33:[1,117],34:[1,118],49:170,50:[1,108],51:[1,112],54:[1,113],56:[1,114],57:[1,115],65:[1,116],66:[1,119],67:[1,120],68:[1,121],69:[1,122],70:[1,123],71:[1,124],72:[1,125],73:[1,126]},{18:[1,110],19:[1,109],20:[1,111],33:[1,117],34:[1,118],49:171,50:[1,108],51:[1,112],54:[1,113],56:[1,114],57:[1,115],65:[1,116],66:[1,119],67:[1,120],68:[1,121],69:[1,122],70:[1,123],71:[1,124],72:[1,125],73:[1,126]},{18:[1,110],19:[1,109],20:[1,111],33:[1,117],34:[1,118],49:172,50:[1,108],51:[1,112],54:[1,113],56:[1,114],57:[1,115],65:[1,116],66:[1,119],67:[1,120],68:[1,121],69:[1,122],70:[1,123],71:[1,124],72:[1,125],73:[1,126]},{5:[2,85],74:[2,85],77:[2,85],81:[2,85]},{5:[2,90],74:[1,54],77:[2,90],81:[2,90]},{5:[2,86],74:[2,86],77:[2,86],81:[2,86]},{5:[2,39],74:[2,39],77:[2,39],81:[2,39]},{5:[2,40],74:[2,40],77:[2,40],81:[2,40]},{5:[2,41],74:[2,41],77:[2,41],81:[2,41]},{5:[2,42],74:[2,42],77:[2,42],81:[2,42]},{5:[2,43],52:[1,173],53:[1,174],74:[2,43],77:[2,43],81:[2,43]},{5:[2,46],52:[1,175],53:[1,176],55:[1,177],74:[2,46],77:[2,46],81:[2,46]},{5:[2,50],52:[1,178],53:[1,179],55:[1,180],74:[2,50],77:[2,50],81:[2,50]},{58:[1,181],61:[1,182],62:[1,183],63:[1,185],64:[1,184]},{5:[2,62],74:[2,62],77:[2,62],81:[2,62]},{5:[2,63],74:[2,63],77:[2,63],81:[2,63]},{5:[2,64],63:[1,186],74:[2,64],77:[2,64],81:[2,64]},{5:[2,66],52:[1,187],53:[1,188],74:[2,66],77:[2,66],81:[2,66]},{5:[2,71],74:[2,71],77:[2,71],81:[2,71]},{5:[2,72],74:[2,72],77:[2,72],81:[2,72]},{5:[2,73],74:[2,73],77:[2,73],81:[2,73]},{5:[2,74],52:[1,189],53:[1,190],55:[1,191],74:[2,74],77:[2,74],81:[2,74]},{5:[2,79],63:[1,192],74:[2,79],77:[2,79],81:[2,79]},{5:[2,81],63:[1,193],74:[2,81],77:[2,81],81:[2,81]},{5:[2,83],63:[1,194],74:[2,83],77:[2,83],81:[2,83]},{5:[2,87],74:[2,87],77:[2,87],81:[2,87]},{5:[2,88],74:[2,88],77:[2,88],81:[2,88]},{5:[2,89],74:[2,89],77:[2,89],81:[2,89]},{5:[2,93],74:[2,93],77:[2,93],81:[2,93]},{5:[2,94],74:[2,94],77:[2,94],81:[2,94]},{5:[2,95],74:[2,95],77:[2,95],81:[2,95]},{5:[2,96],74:[2,96],77:[2,96],81:[2,96]},{5:[2,97],74:[2,97],77:[2,97],81:[2,97]},{5:[2,98],74:[2,98],77:[2,98],81:[2,98]},{5:[2,99],74:[2,99],77:[2,99],81:[2,99]},{5:[2,100],74:[2,100],77:[2,100],81:[2,100]},{5:[2,101],74:[2,101],77:[2,101],81:[2,101]},{5:[2,102],74:[2,102],77:[2,102],81:[2,102]},{5:[2,103],74:[2,103],77:[2,103],81:[2,103]},{5:[2,110],74:[2,110],77:[2,110],81:[2,110]},{5:[2,111],74:[2,111],77:[2,111],81:[2,111]},{5:[2,104],74:[2,104],77:[2,104],81:[2,104]},{5:[2,105],74:[2,105],77:[2,105],81:[2,105]},{5:[2,106],74:[2,106],77:[2,106],81:[2,106]},{5:[2,107],74:[2,107],77:[2,107],81:[2,107]},{5:[2,108],74:[2,108],77:[2,108],81:[2,108]},{5:[2,109],74:[2,109],77:[2,109],81:[2,109]},{5:[2,112],74:[2,112],77:[2,112],81:[2,112]},{5:[2,8],74:[2,8],77:[2,8],81:[2,8]},{5:[2,9],74:[2,9],77:[2,9],81:[2,9]},{5:[2,113],74:[2,113],77:[2,113],81:[2,113]},{5:[2,114],74:[2,114],77:[2,114],81:[2,114]},{5:[2,115],74:[2,115],77:[2,115],81:[2,115]},{5:[2,116],74:[2,116],77:[2,116],81:[2,116]},{5:[2,117],74:[2,117],77:[2,117],81:[2,117]},{5:[2,118],74:[2,118],77:[2,118],81:[2,118]},{5:[2,119],74:[2,119],77:[2,119],81:[2,119]},{5:[2,120],74:[2,120],77:[2,120],81:[2,120]},{5:[2,121],74:[2,121],77:[2,121],81:[2,121]},{5:[2,122],74:[2,122],77:[2,122],81:[2,122]},{5:[2,126],74:[2,126],77:[2,126],81:[2,126]},{5:[2,123],74:[2,123],77:[2,123],81:[2,123]},{5:[2,127],74:[2,127],77:[2,127],81:[2,127]},{5:[2,124],74:[2,124],77:[2,124],81:[2,124]},{5:[2,128],74:[2,128],77:[2,128],81:[2,128]},{5:[2,125],74:[2,125],77:[2,125],81:[2,125]},{5:[2,129],74:[2,129],77:[2,129],81:[2,129]},{5:[2,130],74:[2,130],77:[2,130],81:[2,130]},{5:[2,131],74:[2,131],77:[2,131],81:[2,131]},{5:[2,132],74:[2,132],77:[2,132],81:[2,132]},{5:[2,133],74:[2,133],77:[2,133],81:[2,133]},{5:[2,44],74:[2,44],77:[2,44],81:[2,44]},{5:[2,45],74:[2,45],77:[2,45],81:[2,45]},{5:[2,47],74:[2,47],77:[2,47],81:[2,47]},{5:[2,48],74:[2,48],77:[2,48],81:[2,48]},{5:[2,49],74:[2,49],77:[2,49],81:[2,49]},{5:[2,51],74:[2,51],77:[2,51],81:[2,51]},{5:[2,52],74:[2,52],77:[2,52],81:[2,52]},{5:[2,53],74:[2,53],77:[2,53],81:[2,53]},{59:[1,195],60:[1,196]},{59:[1,197],60:[1,198]},{5:[2,58],63:[1,199],74:[2,58],77:[2,58],81:[2,58]},{5:[2,60],74:[2,60],77:[2,60],81:[2,60]},{5:[2,61],74:[2,61],77:[2,61],81:[2,61]},{5:[2,65],74:[2,65],77:[2,65],81:[2,65]},{5:[2,67],63:[1,200],74:[2,67],77:[2,67],81:[2,67]},{5:[2,69],63:[1,201],74:[2,69],77:[2,69],81:[2,69]},{5:[2,75],74:[2,75],77:[2,75],81:[2,75]},{5:[2,76],74:[2,76],77:[2,76],81:[2,76]},{5:[2,77],63:[1,202],74:[2,77],77:[2,77],81:[2,77]},{5:[2,80],74:[2,80],77:[2,80],81:[2,80]},{5:[2,82],74:[2,82],77:[2,82],81:[2,82]},{5:[2,84],74:[2,84],77:[2,84],81:[2,84]},{5:[2,54],74:[2,54],77:[2,54],81:[2,54]},{5:[2,55],74:[2,55],77:[2,55],81:[2,55]},{5:[2,56],74:[2,56],77:[2,56],81:[2,56]},{5:[2,57],74:[2,57],77:[2,57],81:[2,57]},{5:[2,59],74:[2,59],77:[2,59],81:[2,59]},{5:[2,68],74:[2,68],77:[2,68],81:[2,68]},{5:[2,70],74:[2,70],77:[2,70],81:[2,70]},{5:[2,78],74:[2,78],77:[2,78],81:[2,78]}],
defaultActions: {53:[2,1],68:[2,6],69:[2,7],70:[2,2],71:[2,3],72:[2,4],73:[2,5]},
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
case 9:return 51
break;
case 10:return 33
break;
case 11:return 40
break;
case 12:return 41
break;
case 13:return 42
break;
case 14:return 54
break;
case 15:return 44
break;
case 16:return 45
break;
case 17:return 46
break;
case 18:return 56
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
case 36:return "icmp"
break;
case 37:return "tcp"
break;
case 38:return "udp"
break;
case 39:return "host"
break;
case 40:return "host.cnt"
break;
case 41:return "oldheader"
break;
case 42:return "header"
break;
case 43:return "header.src"
break;
case 44:return "header.src.cnt"
break;
case 45:return "header.dst"
break;
case 46:return "header.dst.cnt"
break;
case 47:return 73
break;
case 48:return 30
break;
case 49:return 50
break;
case 50:return 65
break;
case 51:return 8
break;
case 52:return 7
break;
case 53:return 11
break;
case 54:return 10
break;
case 55:return 76
break;
case 56:return 75
break;
case 57:return 75
break;
case 58:return 77
break;
case 59:return 77
break;
case 60:return 74
break;
case 61:return 74
break;
case 62:return 80
break;
case 63:return 81
break;
case 64:return 78
break;
case 65:return 5
break;
case 66:return 'INVALID'
break;
case 67:console.log(yy_.yytext);
break;
}
};
lexer.rules = [/^(?:\s+)/,/^(?:[0-9]+\b)/,/^(?:([0-9]{1,3})?(\.[0-9]{1,3})?(\.[0-9]{1,3})?(\.[0-9]{1,3})?(\/[0-9]{1,2})?(:[0-9]{1,5})?\b)/,/^(?:bytes)/,/^(?:databytes)/,/^(?:packets)/,/^(?:protocol)/,/^(?:port\.src)/,/^(?:port\.dst)/,/^(?:port)/,/^(?:node)/,/^(?:country\.src)/,/^(?:country\.dst)/,/^(?:country\.xff)/,/^(?:country)/,/^(?:asn\.src)/,/^(?:asn\.dst)/,/^(?:asn\.xff)/,/^(?:asn)/,/^(?:ip\.src)/,/^(?:ip\.dst)/,/^(?:ip\.xff)/,/^(?:ip\.xff\.cnt)/,/^(?:ip)/,/^(?:cert\.issuer\.cn)/,/^(?:cert\.issuer\.on)/,/^(?:cert\.subject\.cn)/,/^(?:cert\.subject\.on)/,/^(?:cert\.alt)/,/^(?:cert\.alt\.cnt)/,/^(?:cert\.serial)/,/^(?:cert\.cnt)/,/^(?:uri)/,/^(?:uri\.cnt)/,/^(?:ua)/,/^(?:ua\.cnt)/,/^(?:icmp)/,/^(?:tcp)/,/^(?:udp)/,/^(?:host)/,/^(?:host\.cnt)/,/^(?:oldheader)/,/^(?:header)/,/^(?:header\.src)/,/^(?:header\.src\.cnt)/,/^(?:header\.dst)/,/^(?:header\.dst\.cnt)/,/^(?:tags)/,/^(?:tags\.cnt)/,/^(?:[/\w*._:-]+)/,/^(?:"[^"]+")/,/^(?:<=)/,/^(?:<)/,/^(?:>=)/,/^(?:>)/,/^(?:!=)/,/^(?:==)/,/^(?:=)/,/^(?:\|\|)/,/^(?:\|)/,/^(?:&&)/,/^(?:&)/,/^(?:\()/,/^(?:\))/,/^(?:!)/,/^(?:$)/,/^(?:.)/,/^(?:.)/];
lexer.conditions = {"INITIAL":{"rules":[0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64,65,66,67],"inclusive":true}};
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