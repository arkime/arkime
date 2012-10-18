/* Jison generated parser */
var molochparser = (function(){
var parser = {trace: function trace() { },
yy: {},
symbols_: {"error":2,"expressions":3,"e":4,"EOF":5,"LTA":6,"lt":7,"lte":8,"GTA":9,"gt":10,"gte":11,"GTLT":12,"IPNUM":13,"IPMATCH":14,"NUMBER":15,"RANGEFIELD":16,"databytes":17,"bytes":18,"packets":19,"protocol":20,"port.src":21,"port.dst":22,"TERMFIELD":23,"node":24,"host":25,"tls.subject.cn":26,"tls.issuer.cn":27,"tls.serial":28,"tls.alt":29,"UPTERMFIELD":30,"country.src":31,"country.dst":32,"country.xff":33,"TEXTFIELD":34,"asn.src":35,"asn.dst":36,"asn.xff":37,"tls.subject.on":38,"tls.issuer.on":39,"STR":40,"ID":41,"port":42,"src":43,"dst":44,"country":45,"xff":46,"asn":47,"tls":48,"issuer":49,"cn":50,"on":51,"subject":52,"alt":53,"serial":54,"QUOTEDSTR":55,"header":56,"icmp":57,"tcp":58,"udp":59,"ip":60,"uri":61,"ua":62,"&&":63,"==":64,"!=":65,"||":66,"!":67,"-":68,"(":69,")":70,"ip.src":71,"ip.dst":72,"ip.xff":73,"tags":74,"$accept":0,"$end":1},
terminals_: {2:"error",5:"EOF",7:"lt",8:"lte",10:"gt",11:"gte",14:"IPMATCH",15:"NUMBER",17:"databytes",18:"bytes",19:"packets",20:"protocol",21:"port.src",22:"port.dst",24:"node",25:"host",26:"tls.subject.cn",27:"tls.issuer.cn",28:"tls.serial",29:"tls.alt",31:"country.src",32:"country.dst",33:"country.xff",35:"asn.src",36:"asn.dst",37:"asn.xff",38:"tls.subject.on",39:"tls.issuer.on",41:"ID",42:"port",43:"src",44:"dst",45:"country",46:"xff",47:"asn",48:"tls",49:"issuer",50:"cn",51:"on",52:"subject",53:"alt",54:"serial",55:"QUOTEDSTR",56:"header",57:"icmp",58:"tcp",59:"udp",60:"ip",61:"uri",62:"ua",63:"&&",64:"==",65:"!=",66:"||",67:"!",68:"-",69:"(",70:")",71:"ip.src",72:"ip.dst",73:"ip.xff",74:"tags"},
productions_: [0,[3,2],[6,1],[6,1],[9,1],[9,1],[12,1],[12,1],[13,1],[13,1],[16,1],[16,1],[16,1],[16,1],[16,1],[16,1],[23,1],[23,1],[23,1],[23,1],[23,1],[23,1],[30,1],[30,1],[30,1],[34,1],[34,1],[34,1],[34,1],[34,1],[40,1],[40,1],[40,1],[40,1],[40,1],[40,2],[40,2],[40,1],[40,2],[40,2],[40,2],[40,1],[40,2],[40,2],[40,2],[40,3],[40,3],[40,3],[40,3],[40,2],[40,2],[40,1],[40,1],[40,1],[40,1],[40,1],[40,1],[40,1],[40,1],[40,2],[40,2],[40,2],[40,1],[40,1],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,2],[4,2],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3]],
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
case 16:this.$ = 'no'
break;
case 17:this.$ = 'ho'
break;
case 18:this.$ = 'tls.sCn'
break;
case 19:this.$ = 'tls.iCn'
break;
case 20:this.$ = 'tls.sn'
break;
case 21:this.$ = 'tls.alt'
break;
case 22:this.$ = 'g1'
break;
case 23:this.$ = 'g2'
break;
case 24:this.$ = 'gxff'
break;
case 25:this.$ = 'as1'
break;
case 26:this.$ = 'as2'
break;
case 27:this.$ = 'asxff'
break;
case 28:this.$ = 'tls.sOn'
break;
case 29:this.$ = 'tls.iOn'
break;
case 64:this.$ = {and: [$$[$0-2], $$[$0]]};
break;
case 65:this.$ = {query: {text: {us: {query: $$[$0], type: "phrase", operator: "and"}}}};
break;
case 66:this.$ = {not: {query: {text: {us: {query: $$[$0], type: "phrase", operator: "and"}}}}};
break;
case 67:this.$ = {query: {text: {ua: {query: $$[$0], type: "phrase", operator: "and"}}}};
break;
case 68:this.$ = {not: {query: {text: {ua: {query: $$[$0], type: "phrase", operator: "and"}}}}};
break;
case 69:this.$ = {or: [$$[$0-2], $$[$0]]};
break;
case 70:this.$ = {not: $$[$0]};
break;
case 71:this.$ = -$$[$0];
break;
case 72:this.$ = $$[$0-1];
break;
case 73:this.$ = {term: {pr: 1}};
break;
case 74:this.$ = {term: {pr: 6}};
break;
case 75:this.$ = {term: {pr: 17}};
break;
case 76:this.$ = {not: {term: {pr: 1}}};
break;
case 77:this.$ = {not: {term: {pr: 6}}};
break;
case 78:this.$ = {not: {term: {pr: 17}}};
break;
case 79:this.$ = {range: {}};
         this.$.range[$$[$0-2]] = {};
         this.$.range[$$[$0-2]][$$[$0-1]] = $$[$0];
break;
case 80:this.$ = {term: {}};
         this.$.term[$$[$0-2]] = $$[$0];
break;
case 81:this.$ = {not: {term: {}}};
         this.$.not.term[$$[$0-2]] = $$[$0];
break;
case 82:this.$ = {or: [{range: {p1: {}}}, {range: {p2: {}}}]};
         this.$.or[0].range.p1[$$[$0-1]] = $$[$0];
         this.$.or[1].range.p2[$$[$0-1]] = $$[$0];
break;
case 83: var str = stripQuotes($$[$0]).toLowerCase();
          if (str.indexOf("*") !== -1) {
            this.$ = {not: {query: {wildcard: {}}}};
            this.$.not.query.wildcard[$$[$0-2]] = str;
          } else {
            this.$ = {not: {term: {}}};
            this.$.not.term[$$[$0-2]] = str;
          }
        
break;
case 84: var str = stripQuotes($$[$0]).toLowerCase();
          if (str.indexOf("*") !== -1) {
            this.$ = {query: {wildcard: {}}};
            this.$.query.wildcard[$$[$0-2]] = str;
          } else {
            this.$ = {term: {}};
            this.$.term[$$[$0-2]] = str;
          }
        
break;
case 85: var str = stripQuotes($$[$0]).toUpperCase();
          if (str.indexOf("*") !== -1) {
            this.$ = {not: {query: {wildcard: {}}}};
            this.$.not.query.wildcard[$$[$0-2]] = str;
          } else {
            this.$ = {not: {term: {}}};
            this.$.not.term[$$[$0-2]] = str;
          }
        
break;
case 86: var str = stripQuotes($$[$0]).toUpperCase();
          if (str.indexOf("*") !== -1) {
            this.$ = {query: {wildcard: {}}};
            this.$.query.wildcard[$$[$0-2]] = str;
          } else {
            this.$ = {term: {}};
            this.$.term[$$[$0-2]] = str;
          }
        
break;
case 87: var str = stripQuotes($$[$0]).toLowerCase();
          if (str.indexOf("*") !== -1) {
            this.$ = {not: {query: {wildcard: {}}}};
            this.$.not.query.wildcard[$$[$0-2]] = str;
          } else {
            this.$ = {not: {query: {text: {}}}};
            this.$.not.query.text[$$[$0-2]] = {query: str, type: "phrase", operator: "and"}
          }
        
break;
case 88: var str = stripQuotes($$[$0]).toLowerCase();
          if (str.indexOf("*") !== -1) {
            this.$ = {query: {wildcard: {}}};
            this.$.query.wildcard[$$[$0-2]] = str;
          } else {
            this.$ = {query: {text: {}}};
            this.$.query.text[$$[$0-2]] = {query: str, type: "phrase", operator: "and"}
          }
        
break;
case 89:this.$ = {or: [{term: {p1: $$[$0]}}, {term: {p2: $$[$0]}}]};
break;
case 90:this.$ = {not: {or: [{term: {p1: $$[$0]}}, {term: {p2: $$[$0]}}]}};
break;
case 91:this.$ = parseIpPort($$[$0],0);
break;
case 92:this.$ = {not: parseIpPort($$[$0],0)};
break;
case 93:this.$ = parseIpPort($$[$0],1);
break;
case 94:this.$ = {not: parseIpPort($$[$0],1)};
break;
case 95:this.$ = parseIpPort($$[$0],2);
break;
case 96:this.$ = {not: parseIpPort($$[$0],2)};
break;
case 97:this.$ = parseIpPort($$[$0],3);
break;
case 98:this.$ = {not: parseIpPort($$[$0],3)};
break;
case 99: var tag = stripQuotes($$[$0]);
          this.$ = {term: {ta: tag}};
        
break;
case 100: var tag = stripQuotes($$[$0]);
          this.$ = {not: {term: {ta: tag}}};
        
break;
case 101: var tag = stripQuotes($$[$0]);
          this.$ = {term: {hh: tag}};
        
break;
case 102: var tag = stripQuotes($$[$0]);
          this.$ = {not: {term: {hh: tag}}};
        
break;
case 103: var str = stripQuotes($$[$0]).toUpperCase();
          if (str.indexOf("*") !== -1) {
            this.$ = {or: [{query: {wildcard: {g1: str}}}, {query: {wildcard: {g2: str}}}, {query: {wildcard: {gxff: str}}}]};
          } else {
            this.$ = {or: [{term: {g1: str}}, {term: {g2: str}}, {term: {gxff: str}}]};
          }
        
break;
case 104: var str = stripQuotes($$[$0]).toUpperCase();
          if (str.indexOf("*") !== -1) {
            this.$ = {not: {or: [{query: {wildcard: {g1: str}}}, {query: {wildcard: {g2: str}}}, {query: {wildcard: {gxff: str}}}]}};
          } else {
            this.$ = {not: {or: [{term: {g1: str}}, {term: {g2: str}}, {term: {gxff: str}}]}};
          }
        
break;
case 105: var str = stripQuotes($$[$0]).toLowerCase();
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
case 106: var str = stripQuotes($$[$0]).toLowerCase();
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
table: [{3:1,4:2,16:9,17:[1,22],18:[1,23],19:[1,24],20:[1,8],21:[1,25],22:[1,26],23:11,24:[1,27],25:[1,28],26:[1,29],27:[1,30],28:[1,31],29:[1,32],30:12,31:[1,33],32:[1,34],33:[1,35],34:13,35:[1,36],36:[1,37],37:[1,38],38:[1,39],39:[1,40],42:[1,10],45:[1,20],47:[1,21],56:[1,19],60:[1,14],61:[1,3],62:[1,4],67:[1,5],68:[1,6],69:[1,7],71:[1,15],72:[1,16],73:[1,17],74:[1,18]},{1:[3]},{5:[1,41],63:[1,42],66:[1,43]},{64:[1,44],65:[1,45]},{64:[1,46],65:[1,47]},{4:48,16:9,17:[1,22],18:[1,23],19:[1,24],20:[1,8],21:[1,25],22:[1,26],23:11,24:[1,27],25:[1,28],26:[1,29],27:[1,30],28:[1,31],29:[1,32],30:12,31:[1,33],32:[1,34],33:[1,35],34:13,35:[1,36],36:[1,37],37:[1,38],38:[1,39],39:[1,40],42:[1,10],45:[1,20],47:[1,21],56:[1,19],60:[1,14],61:[1,3],62:[1,4],67:[1,5],68:[1,6],69:[1,7],71:[1,15],72:[1,16],73:[1,17],74:[1,18]},{4:49,16:9,17:[1,22],18:[1,23],19:[1,24],20:[1,8],21:[1,25],22:[1,26],23:11,24:[1,27],25:[1,28],26:[1,29],27:[1,30],28:[1,31],29:[1,32],30:12,31:[1,33],32:[1,34],33:[1,35],34:13,35:[1,36],36:[1,37],37:[1,38],38:[1,39],39:[1,40],42:[1,10],45:[1,20],47:[1,21],56:[1,19],60:[1,14],61:[1,3],62:[1,4],67:[1,5],68:[1,6],69:[1,7],71:[1,15],72:[1,16],73:[1,17],74:[1,18]},{4:50,16:9,17:[1,22],18:[1,23],19:[1,24],20:[1,8],21:[1,25],22:[1,26],23:11,24:[1,27],25:[1,28],26:[1,29],27:[1,30],28:[1,31],29:[1,32],30:12,31:[1,33],32:[1,34],33:[1,35],34:13,35:[1,36],36:[1,37],37:[1,38],38:[1,39],39:[1,40],42:[1,10],45:[1,20],47:[1,21],56:[1,19],60:[1,14],61:[1,3],62:[1,4],67:[1,5],68:[1,6],69:[1,7],71:[1,15],72:[1,16],73:[1,17],74:[1,18]},{7:[2,13],8:[2,13],10:[2,13],11:[2,13],64:[1,51],65:[1,52]},{6:56,7:[1,58],8:[1,59],9:57,10:[1,60],11:[1,61],12:53,64:[1,54],65:[1,55]},{6:56,7:[1,58],8:[1,59],9:57,10:[1,60],11:[1,61],12:62,64:[1,63],65:[1,64]},{64:[1,66],65:[1,65]},{64:[1,68],65:[1,67]},{64:[1,70],65:[1,69]},{64:[1,71],65:[1,72]},{64:[1,73],65:[1,74]},{64:[1,75],65:[1,76]},{64:[1,77],65:[1,78]},{64:[1,79],65:[1,80]},{64:[1,81],65:[1,82]},{64:[1,83],65:[1,84]},{64:[1,85],65:[1,86]},{7:[2,10],8:[2,10],10:[2,10],11:[2,10],64:[2,10],65:[2,10]},{7:[2,11],8:[2,11],10:[2,11],11:[2,11],64:[2,11],65:[2,11]},{7:[2,12],8:[2,12],10:[2,12],11:[2,12],64:[2,12],65:[2,12]},{7:[2,14],8:[2,14],10:[2,14],11:[2,14],64:[2,14],65:[2,14]},{7:[2,15],8:[2,15],10:[2,15],11:[2,15],64:[2,15],65:[2,15]},{64:[2,16],65:[2,16]},{64:[2,17],65:[2,17]},{64:[2,18],65:[2,18]},{64:[2,19],65:[2,19]},{64:[2,20],65:[2,20]},{64:[2,21],65:[2,21]},{64:[2,22],65:[2,22]},{64:[2,23],65:[2,23]},{64:[2,24],65:[2,24]},{64:[2,25],65:[2,25]},{64:[2,26],65:[2,26]},{64:[2,27],65:[2,27]},{64:[2,28],65:[2,28]},{64:[2,29],65:[2,29]},{1:[2,1]},{4:87,16:9,17:[1,22],18:[1,23],19:[1,24],20:[1,8],21:[1,25],22:[1,26],23:11,24:[1,27],25:[1,28],26:[1,29],27:[1,30],28:[1,31],29:[1,32],30:12,31:[1,33],32:[1,34],33:[1,35],34:13,35:[1,36],36:[1,37],37:[1,38],38:[1,39],39:[1,40],42:[1,10],45:[1,20],47:[1,21],56:[1,19],60:[1,14],61:[1,3],62:[1,4],67:[1,5],68:[1,6],69:[1,7],71:[1,15],72:[1,16],73:[1,17],74:[1,18]},{4:88,16:9,17:[1,22],18:[1,23],19:[1,24],20:[1,8],21:[1,25],22:[1,26],23:11,24:[1,27],25:[1,28],26:[1,29],27:[1,30],28:[1,31],29:[1,32],30:12,31:[1,33],32:[1,34],33:[1,35],34:13,35:[1,36],36:[1,37],37:[1,38],38:[1,39],39:[1,40],42:[1,10],45:[1,20],47:[1,21],56:[1,19],60:[1,14],61:[1,3],62:[1,4],67:[1,5],68:[1,6],69:[1,7],71:[1,15],72:[1,16],73:[1,17],74:[1,18]},{18:[1,92],19:[1,91],20:[1,93],24:[1,99],25:[1,100],40:89,41:[1,90],42:[1,94],45:[1,95],47:[1,96],48:[1,97],55:[1,98],56:[1,101],57:[1,102],58:[1,103],59:[1,104],60:[1,105],61:[1,106],62:[1,107]},{18:[1,92],19:[1,91],20:[1,93],24:[1,99],25:[1,100],40:108,41:[1,90],42:[1,94],45:[1,95],47:[1,96],48:[1,97],55:[1,98],56:[1,101],57:[1,102],58:[1,103],59:[1,104],60:[1,105],61:[1,106],62:[1,107]},{18:[1,92],19:[1,91],20:[1,93],24:[1,99],25:[1,100],40:109,41:[1,90],42:[1,94],45:[1,95],47:[1,96],48:[1,97],55:[1,98],56:[1,101],57:[1,102],58:[1,103],59:[1,104],60:[1,105],61:[1,106],62:[1,107]},{18:[1,92],19:[1,91],20:[1,93],24:[1,99],25:[1,100],40:110,41:[1,90],42:[1,94],45:[1,95],47:[1,96],48:[1,97],55:[1,98],56:[1,101],57:[1,102],58:[1,103],59:[1,104],60:[1,105],61:[1,106],62:[1,107]},{5:[2,70],63:[2,70],66:[2,70],70:[2,70]},{5:[2,71],63:[2,71],66:[2,71],70:[2,71]},{63:[1,42],66:[1,43],70:[1,111]},{57:[1,112],58:[1,113],59:[1,114]},{57:[1,115],58:[1,116],59:[1,117]},{15:[1,118]},{15:[1,119]},{15:[1,120]},{15:[2,6]},{15:[2,7]},{15:[2,2]},{15:[2,3]},{15:[2,4]},{15:[2,5]},{15:[1,121]},{15:[1,122]},{15:[1,123]},{18:[1,92],19:[1,91],20:[1,93],24:[1,99],25:[1,100],40:124,41:[1,90],42:[1,94],45:[1,95],47:[1,96],48:[1,97],55:[1,98],56:[1,101],57:[1,102],58:[1,103],59:[1,104],60:[1,105],61:[1,106],62:[1,107]},{18:[1,92],19:[1,91],20:[1,93],24:[1,99],25:[1,100],40:125,41:[1,90],42:[1,94],45:[1,95],47:[1,96],48:[1,97],55:[1,98],56:[1,101],57:[1,102],58:[1,103],59:[1,104],60:[1,105],61:[1,106],62:[1,107]},{18:[1,92],19:[1,91],20:[1,93],24:[1,99],25:[1,100],40:126,41:[1,90],42:[1,94],45:[1,95],47:[1,96],48:[1,97],55:[1,98],56:[1,101],57:[1,102],58:[1,103],59:[1,104],60:[1,105],61:[1,106],62:[1,107]},{18:[1,92],19:[1,91],20:[1,93],24:[1,99],25:[1,100],40:127,41:[1,90],42:[1,94],45:[1,95],47:[1,96],48:[1,97],55:[1,98],56:[1,101],57:[1,102],58:[1,103],59:[1,104],60:[1,105],61:[1,106],62:[1,107]},{18:[1,92],19:[1,91],20:[1,93],24:[1,99],25:[1,100],40:128,41:[1,90],42:[1,94],45:[1,95],47:[1,96],48:[1,97],55:[1,98],56:[1,101],57:[1,102],58:[1,103],59:[1,104],60:[1,105],61:[1,106],62:[1,107]},{18:[1,92],19:[1,91],20:[1,93],24:[1,99],25:[1,100],40:129,41:[1,90],42:[1,94],45:[1,95],47:[1,96],48:[1,97],55:[1,98],56:[1,101],57:[1,102],58:[1,103],59:[1,104],60:[1,105],61:[1,106],62:[1,107]},{13:130,14:[1,131],15:[1,132]},{13:133,14:[1,131],15:[1,132]},{13:134,14:[1,131],15:[1,132]},{13:135,14:[1,131],15:[1,132]},{13:136,14:[1,131],15:[1,132]},{13:137,14:[1,131],15:[1,132]},{13:138,14:[1,131],15:[1,132]},{13:139,14:[1,131],15:[1,132]},{18:[1,92],19:[1,91],20:[1,93],24:[1,99],25:[1,100],40:140,41:[1,90],42:[1,94],45:[1,95],47:[1,96],48:[1,97],55:[1,98],56:[1,101],57:[1,102],58:[1,103],59:[1,104],60:[1,105],61:[1,106],62:[1,107]},{18:[1,92],19:[1,91],20:[1,93],24:[1,99],25:[1,100],40:141,41:[1,90],42:[1,94],45:[1,95],47:[1,96],48:[1,97],55:[1,98],56:[1,101],57:[1,102],58:[1,103],59:[1,104],60:[1,105],61:[1,106],62:[1,107]},{18:[1,92],19:[1,91],20:[1,93],24:[1,99],25:[1,100],40:142,41:[1,90],42:[1,94],45:[1,95],47:[1,96],48:[1,97],55:[1,98],56:[1,101],57:[1,102],58:[1,103],59:[1,104],60:[1,105],61:[1,106],62:[1,107]},{18:[1,92],19:[1,91],20:[1,93],24:[1,99],25:[1,100],40:143,41:[1,90],42:[1,94],45:[1,95],47:[1,96],48:[1,97],55:[1,98],56:[1,101],57:[1,102],58:[1,103],59:[1,104],60:[1,105],61:[1,106],62:[1,107]},{18:[1,92],19:[1,91],20:[1,93],24:[1,99],25:[1,100],40:144,41:[1,90],42:[1,94],45:[1,95],47:[1,96],48:[1,97],55:[1,98],56:[1,101],57:[1,102],58:[1,103],59:[1,104],60:[1,105],61:[1,106],62:[1,107]},{18:[1,92],19:[1,91],20:[1,93],24:[1,99],25:[1,100],40:145,41:[1,90],42:[1,94],45:[1,95],47:[1,96],48:[1,97],55:[1,98],56:[1,101],57:[1,102],58:[1,103],59:[1,104],60:[1,105],61:[1,106],62:[1,107]},{18:[1,92],19:[1,91],20:[1,93],24:[1,99],25:[1,100],40:146,41:[1,90],42:[1,94],45:[1,95],47:[1,96],48:[1,97],55:[1,98],56:[1,101],57:[1,102],58:[1,103],59:[1,104],60:[1,105],61:[1,106],62:[1,107]},{18:[1,92],19:[1,91],20:[1,93],24:[1,99],25:[1,100],40:147,41:[1,90],42:[1,94],45:[1,95],47:[1,96],48:[1,97],55:[1,98],56:[1,101],57:[1,102],58:[1,103],59:[1,104],60:[1,105],61:[1,106],62:[1,107]},{5:[2,64],63:[2,64],66:[2,64],70:[2,64]},{5:[2,69],63:[1,42],66:[2,69],70:[2,69]},{5:[2,65],63:[2,65],66:[2,65],70:[2,65]},{5:[2,30],63:[2,30],66:[2,30],70:[2,30]},{5:[2,31],63:[2,31],66:[2,31],70:[2,31]},{5:[2,32],63:[2,32],66:[2,32],70:[2,32]},{5:[2,33],63:[2,33],66:[2,33],70:[2,33]},{5:[2,34],43:[1,148],44:[1,149],63:[2,34],66:[2,34],70:[2,34]},{5:[2,37],43:[1,150],44:[1,151],46:[1,152],63:[2,37],66:[2,37],70:[2,37]},{5:[2,41],43:[1,153],44:[1,154],46:[1,155],63:[2,41],66:[2,41],70:[2,41]},{49:[1,156],52:[1,157],53:[1,158],54:[1,159]},{5:[2,51],63:[2,51],66:[2,51],70:[2,51]},{5:[2,52],63:[2,52],66:[2,52],70:[2,52]},{5:[2,53],63:[2,53],66:[2,53],70:[2,53]},{5:[2,54],63:[2,54],66:[2,54],70:[2,54]},{5:[2,55],63:[2,55],66:[2,55],70:[2,55]},{5:[2,56],63:[2,56],66:[2,56],70:[2,56]},{5:[2,57],63:[2,57],66:[2,57],70:[2,57]},{5:[2,58],43:[1,160],44:[1,161],46:[1,162],63:[2,58],66:[2,58],70:[2,58]},{5:[2,62],63:[2,62],66:[2,62],70:[2,62]},{5:[2,63],63:[2,63],66:[2,63],70:[2,63]},{5:[2,66],63:[2,66],66:[2,66],70:[2,66]},{5:[2,67],63:[2,67],66:[2,67],70:[2,67]},{5:[2,68],63:[2,68],66:[2,68],70:[2,68]},{5:[2,72],63:[2,72],66:[2,72],70:[2,72]},{5:[2,73],63:[2,73],66:[2,73],70:[2,73]},{5:[2,74],63:[2,74],66:[2,74],70:[2,74]},{5:[2,75],63:[2,75],66:[2,75],70:[2,75]},{5:[2,76],63:[2,76],66:[2,76],70:[2,76]},{5:[2,77],63:[2,77],66:[2,77],70:[2,77]},{5:[2,78],63:[2,78],66:[2,78],70:[2,78]},{5:[2,79],63:[2,79],66:[2,79],70:[2,79]},{5:[2,80],63:[2,80],66:[2,80],70:[2,80]},{5:[2,81],63:[2,81],66:[2,81],70:[2,81]},{5:[2,82],63:[2,82],66:[2,82],70:[2,82]},{5:[2,89],63:[2,89],66:[2,89],70:[2,89]},{5:[2,90],63:[2,90],66:[2,90],70:[2,90]},{5:[2,83],63:[2,83],66:[2,83],70:[2,83]},{5:[2,84],63:[2,84],66:[2,84],70:[2,84]},{5:[2,85],63:[2,85],66:[2,85],70:[2,85]},{5:[2,86],63:[2,86],66:[2,86],70:[2,86]},{5:[2,87],63:[2,87],66:[2,87],70:[2,87]},{5:[2,88],63:[2,88],66:[2,88],70:[2,88]},{5:[2,91],63:[2,91],66:[2,91],70:[2,91]},{5:[2,8],63:[2,8],66:[2,8],70:[2,8]},{5:[2,9],63:[2,9],66:[2,9],70:[2,9]},{5:[2,92],63:[2,92],66:[2,92],70:[2,92]},{5:[2,93],63:[2,93],66:[2,93],70:[2,93]},{5:[2,94],63:[2,94],66:[2,94],70:[2,94]},{5:[2,95],63:[2,95],66:[2,95],70:[2,95]},{5:[2,96],63:[2,96],66:[2,96],70:[2,96]},{5:[2,97],63:[2,97],66:[2,97],70:[2,97]},{5:[2,98],63:[2,98],66:[2,98],70:[2,98]},{5:[2,99],63:[2,99],66:[2,99],70:[2,99]},{5:[2,100],63:[2,100],66:[2,100],70:[2,100]},{5:[2,101],63:[2,101],66:[2,101],70:[2,101]},{5:[2,102],63:[2,102],66:[2,102],70:[2,102]},{5:[2,103],63:[2,103],66:[2,103],70:[2,103]},{5:[2,104],63:[2,104],66:[2,104],70:[2,104]},{5:[2,105],63:[2,105],66:[2,105],70:[2,105]},{5:[2,106],63:[2,106],66:[2,106],70:[2,106]},{5:[2,35],63:[2,35],66:[2,35],70:[2,35]},{5:[2,36],63:[2,36],66:[2,36],70:[2,36]},{5:[2,38],63:[2,38],66:[2,38],70:[2,38]},{5:[2,39],63:[2,39],66:[2,39],70:[2,39]},{5:[2,40],63:[2,40],66:[2,40],70:[2,40]},{5:[2,42],63:[2,42],66:[2,42],70:[2,42]},{5:[2,43],63:[2,43],66:[2,43],70:[2,43]},{5:[2,44],63:[2,44],66:[2,44],70:[2,44]},{50:[1,163],51:[1,164]},{50:[1,165],51:[1,166]},{5:[2,49],63:[2,49],66:[2,49],70:[2,49]},{5:[2,50],63:[2,50],66:[2,50],70:[2,50]},{5:[2,59],63:[2,59],66:[2,59],70:[2,59]},{5:[2,60],63:[2,60],66:[2,60],70:[2,60]},{5:[2,61],63:[2,61],66:[2,61],70:[2,61]},{5:[2,45],63:[2,45],66:[2,45],70:[2,45]},{5:[2,46],63:[2,46],66:[2,46],70:[2,46]},{5:[2,47],63:[2,47],66:[2,47],70:[2,47]},{5:[2,48],63:[2,48],66:[2,48],70:[2,48]}],
defaultActions: {41:[2,1],56:[2,6],57:[2,7],58:[2,2],59:[2,3],60:[2,4],61:[2,5]},
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
case 9:return 42
break;
case 10:return 24
break;
case 11:return 31
break;
case 12:return 32
break;
case 13:return 33
break;
case 14:return 45
break;
case 15:return 35
break;
case 16:return 36
break;
case 17:return 37
break;
case 18:return 47
break;
case 19:return "ip.src"
break;
case 20:return "ip.dst"
break;
case 21:return "ip.xff"
break;
case 22:return "ip"
break;
case 23:return "tls.issuer.cn"
break;
case 24:return "tls.issuer.on"
break;
case 25:return "tls.subject.cn"
break;
case 26:return "tls.subject.on"
break;
case 27:return "tls.alt"
break;
case 28:return "tls.serial"
break;
case 29:return "uri"
break;
case 30:return "ua"
break;
case 31:return "icmp"
break;
case 32:return "tcp"
break;
case 33:return "udp"
break;
case 34:return "host"
break;
case 35:return "header"
break;
case 36:return 74
break;
case 37:return 41
break;
case 38:return 55
break;
case 39:return 8
break;
case 40:return 7
break;
case 41:return 11
break;
case 42:return 10
break;
case 43:return 65
break;
case 44:return 64
break;
case 45:return 64
break;
case 46:return 66
break;
case 47:return 66
break;
case 48:return 63
break;
case 49:return 63
break;
case 50:return 69
break;
case 51:return 70
break;
case 52:return 67
break;
case 53:return 5
break;
case 54:return 'INVALID'
break;
case 55:console.log(yy_.yytext);
break;
}
};
lexer.rules = [/^(?:\s+)/,/^(?:[0-9]+\b)/,/^(?:([0-9]{1,3})?(\.[0-9]{1,3})?(\.[0-9]{1,3})?(\.[0-9]{1,3})?(\/[0-9]{1,2})?(:[0-9]{1,5})?\b)/,/^(?:bytes)/,/^(?:databytes)/,/^(?:packets)/,/^(?:protocol)/,/^(?:port\.src)/,/^(?:port\.dst)/,/^(?:port)/,/^(?:node)/,/^(?:country\.src)/,/^(?:country\.dst)/,/^(?:country\.xff)/,/^(?:country)/,/^(?:asn\.src)/,/^(?:asn\.dst)/,/^(?:asn\.xff)/,/^(?:asn)/,/^(?:ip\.src)/,/^(?:ip\.dst)/,/^(?:ip\.xff)/,/^(?:ip)/,/^(?:tls\.issuer\.cn)/,/^(?:tls\.issuer\.on)/,/^(?:tls\.subject\.cn)/,/^(?:tls\.subject\.on)/,/^(?:tls\.alt)/,/^(?:tls\.serial)/,/^(?:uri)/,/^(?:ua)/,/^(?:icmp)/,/^(?:tcp)/,/^(?:udp)/,/^(?:host)/,/^(?:header)/,/^(?:tags)/,/^(?:[/\w*._:-]+)/,/^(?:"[^"]+")/,/^(?:<=)/,/^(?:<)/,/^(?:>=)/,/^(?:>)/,/^(?:!=)/,/^(?:==)/,/^(?:=)/,/^(?:\|\|)/,/^(?:\|)/,/^(?:&&)/,/^(?:&)/,/^(?:\()/,/^(?:\))/,/^(?:!)/,/^(?:$)/,/^(?:.)/,/^(?:.)/];
lexer.conditions = {"INITIAL":{"rules":[0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55],"inclusive":true}};
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