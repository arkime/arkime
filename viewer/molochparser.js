/* Jison generated parser */
var molochparser = (function(){
var parser = {trace: function trace() { },
yy: {},
symbols_: {"error":2,"expressions":3,"e":4,"EOF":5,"LTA":6,"lt":7,"lte":8,"GTA":9,"gt":10,"gte":11,"GTLT":12,"IPNUM":13,"IPMATCH":14,"NUMBER":15,"RANGEFIELD":16,"databytes":17,"bytes":18,"packets":19,"protocol":20,"port1":21,"port2":22,"STRFIELD":23,"country1":24,"country2":25,"node":26,"host":27,"STR":28,"ID":29,"port":30,"country":31,"QUOTEDSTR":32,"header":33,"xff":34,"tcp":35,"contains":36,"udp":37,"ip":38,"ip1":39,"ip2":40,"uri":41,"ua":42,"&&":43,"||":44,"!":45,"-":46,"(":47,")":48,"==":49,"!=":50,"tags":51,"$accept":0,"$end":1},
terminals_: {2:"error",5:"EOF",7:"lt",8:"lte",10:"gt",11:"gte",14:"IPMATCH",15:"NUMBER",17:"databytes",18:"bytes",19:"packets",20:"protocol",21:"port1",22:"port2",24:"country1",25:"country2",26:"node",27:"host",29:"ID",30:"port",31:"country",32:"QUOTEDSTR",33:"header",34:"xff",35:"tcp",36:"contains",37:"udp",38:"ip",39:"ip1",40:"ip2",41:"uri",42:"ua",43:"&&",44:"||",45:"!",46:"-",47:"(",48:")",49:"==",50:"!=",51:"tags"},
productions_: [0,[3,2],[6,1],[6,1],[9,1],[9,1],[12,1],[12,1],[13,1],[13,1],[16,1],[16,1],[16,1],[16,1],[16,1],[16,1],[23,1],[23,1],[23,1],[23,1],[28,1],[28,1],[28,1],[28,1],[28,1],[28,1],[28,1],[28,1],[28,1],[28,1],[28,1],[28,1],[28,1],[28,1],[28,1],[28,1],[28,1],[28,1],[28,1],[28,1],[28,1],[28,1],[28,1],[4,3],[4,3],[4,3],[4,3],[4,2],[4,2],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3]],
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
case 16:this.$ = 'g1'
break;
case 17:this.$ = 'g2'
break;
case 18:this.$ = 'no'
break;
case 19:this.$ = 'ho'
break;
case 43:this.$ = {and: [$$[$0-2], $$[$0]]};
break;
case 44:this.$ = {query: {text: {us: {query: $$[$0], type: "phrase", operator: "and"}}}};
break;
case 45:this.$ = {query: {text: {ua: {query: $$[$0], type: "phrase", operator: "and"}}}};
break;
case 46:this.$ = {or: [$$[$0-2], $$[$0]]};
break;
case 47:this.$ = {not: $$[$0]};
break;
case 48:this.$ = -$$[$0];
break;
case 49:this.$ = $$[$0-1];
break;
case 50:this.$ = {term: {pr: 6}};
break;
case 51:this.$ = {term: {pr: 17}};
break;
case 52:this.$ = {range: {}};
         this.$.range[$$[$0-2]] = {};
         this.$.range[$$[$0-2]][$$[$0-1]] = $$[$0];
break;
case 53:this.$ = {term: {}};
         this.$.term[$$[$0-2]] = $$[$0];
break;
case 54:this.$ = {not: {term: {}}};
         this.$.not.term[$$[$0-2]] = $$[$0];
break;
case 55:this.$ = {or: [{range: {p1: {}}}, {range: {p2: {}}}]};
         this.$.or[0].range.p1[$$[$0-1]] = $$[$0];
         this.$.or[1].range.p2[$$[$0-1]] = $$[$0];
break;
case 56:this.$ = {term: {}};
         this.$.term[$$[$0-2]] = $$[$0];
break;
case 57:this.$ = {not: {term: {}}};
         this.$.not.term[$$[$0-2]] = $$[$0];
break;
case 58: var str = stripQuotes($$[$0]);
          if (str.indexOf("*") !== -1) {
            this.$ = {query: {wildcard: {}}};
            this.$.query.wildcard[$$[$0-2]] = str;
          } else {
            this.$ = {term: {}};
            this.$.term[$$[$0-2]] = str;
          }
        
break;
case 59:this.$ = {or: [{term: {p1: $$[$0]}}, {term: {p2: $$[$0]}}]};
break;
case 60:this.$ = {not: {or: [{term: {p1: $$[$0]}}, {term: {p2: $$[$0]}}]}};
break;
case 61:this.$ = parseIpPort($$[$0],0);
break;
case 62:this.$ = {not: parseIpPort($$[$0],0)};
break;
case 63:this.$ = parseIpPort($$[$0],1);
break;
case 64:this.$ = {not: parseIpPort($$[$0],1)};
break;
case 65:this.$ = parseIpPort($$[$0],2);
break;
case 66:this.$ = {not: parseIpPort($$[$0],2)};
break;
case 67:this.$ = parseIpPort($$[$0],3);
break;
case 68:this.$ = {not: parseIpPort($$[$0],3)};
break;
case 69: var tag = stripQuotes($$[$0]);
          this.$ = {term: {ta: tag}};
        
break;
case 70: var tag = stripQuotes($$[$0]);
          this.$ = {not: {term: {ta: tag}}};
        
break;
case 71: var tag = stripQuotes($$[$0]);
          this.$ = {term: {ta: tag}};
        
break;
case 72: var tag = stripQuotes($$[$0]);
          this.$ = {term: {hh: tag}};
        
break;
case 73: var tag = stripQuotes($$[$0]);
          this.$ = {not: {term: {hh: tag}}};
        
break;
case 74: var tag = stripQuotes($$[$0]);
          this.$ = {term: {hh: tag}};
        
break;
case 75:this.$ = {or: [{term: {g1: $$[$0].toUpperCase()}}, {term: {g2: $$[$0].toUpperCase()}}]};
break;
case 76:this.$ = {not: {or: [{term: {g1: $$[$0].toUpperCase()}}, {term: {g2: $$[$0].toUpperCase()}}]}};
break;
case 77:this.$ = {or: [{query: {wildcard: {g1: $$[$0].toUpperCase()}}}, {query: {wildcard: {g2: $$[$0].toUpperCase()}}}]};
break;
case 78: var str = stripQuotes($$[$0]);
          if (str.indexOf("*") !== -1) {
            this.$ = {query: {wildcard: {}}};
            this.$.query.wildcard[$$[$0-2]] = str;
          } else {
            this.$ = {term: {}};
            this.$.term[$$[$0-2]] = str;
          }
        
break;
}
},
table: [{3:1,4:2,16:9,17:[1,19],18:[1,20],19:[1,21],20:[1,8],21:[1,22],22:[1,23],23:11,24:[1,24],25:[1,25],26:[1,26],27:[1,27],30:[1,10],31:[1,18],33:[1,17],34:[1,15],38:[1,12],39:[1,13],40:[1,14],41:[1,3],42:[1,4],45:[1,5],46:[1,6],47:[1,7],51:[1,16]},{1:[3]},{5:[1,28],43:[1,29],44:[1,30]},{36:[1,31]},{36:[1,32]},{4:33,16:9,17:[1,19],18:[1,20],19:[1,21],20:[1,8],21:[1,22],22:[1,23],23:11,24:[1,24],25:[1,25],26:[1,26],27:[1,27],30:[1,10],31:[1,18],33:[1,17],34:[1,15],38:[1,12],39:[1,13],40:[1,14],41:[1,3],42:[1,4],45:[1,5],46:[1,6],47:[1,7],51:[1,16]},{4:34,16:9,17:[1,19],18:[1,20],19:[1,21],20:[1,8],21:[1,22],22:[1,23],23:11,24:[1,24],25:[1,25],26:[1,26],27:[1,27],30:[1,10],31:[1,18],33:[1,17],34:[1,15],38:[1,12],39:[1,13],40:[1,14],41:[1,3],42:[1,4],45:[1,5],46:[1,6],47:[1,7],51:[1,16]},{4:35,16:9,17:[1,19],18:[1,20],19:[1,21],20:[1,8],21:[1,22],22:[1,23],23:11,24:[1,24],25:[1,25],26:[1,26],27:[1,27],30:[1,10],31:[1,18],33:[1,17],34:[1,15],38:[1,12],39:[1,13],40:[1,14],41:[1,3],42:[1,4],45:[1,5],46:[1,6],47:[1,7],51:[1,16]},{7:[2,13],8:[2,13],10:[2,13],11:[2,13],49:[1,36],50:[2,13]},{6:40,7:[1,42],8:[1,43],9:41,10:[1,44],11:[1,45],12:37,49:[1,38],50:[1,39]},{6:40,7:[1,42],8:[1,43],9:41,10:[1,44],11:[1,45],12:46,49:[1,47],50:[1,48]},{36:[1,51],49:[1,49],50:[1,50]},{49:[1,52],50:[1,53]},{49:[1,54],50:[1,55]},{49:[1,56],50:[1,57]},{49:[1,58],50:[1,59]},{36:[1,62],49:[1,60],50:[1,61]},{36:[1,65],49:[1,63],50:[1,64]},{36:[1,68],49:[1,66],50:[1,67]},{7:[2,10],8:[2,10],10:[2,10],11:[2,10],49:[2,10],50:[2,10]},{7:[2,11],8:[2,11],10:[2,11],11:[2,11],49:[2,11],50:[2,11]},{7:[2,12],8:[2,12],10:[2,12],11:[2,12],49:[2,12],50:[2,12]},{7:[2,14],8:[2,14],10:[2,14],11:[2,14],49:[2,14],50:[2,14]},{7:[2,15],8:[2,15],10:[2,15],11:[2,15],49:[2,15],50:[2,15]},{36:[2,16],49:[2,16],50:[2,16]},{36:[2,17],49:[2,17],50:[2,17]},{36:[2,18],49:[2,18],50:[2,18]},{36:[2,19],49:[2,19],50:[2,19]},{1:[2,1]},{4:69,16:9,17:[1,19],18:[1,20],19:[1,21],20:[1,8],21:[1,22],22:[1,23],23:11,24:[1,24],25:[1,25],26:[1,26],27:[1,27],30:[1,10],31:[1,18],33:[1,17],34:[1,15],38:[1,12],39:[1,13],40:[1,14],41:[1,3],42:[1,4],45:[1,5],46:[1,6],47:[1,7],51:[1,16]},{4:70,16:9,17:[1,19],18:[1,20],19:[1,21],20:[1,8],21:[1,22],22:[1,23],23:11,24:[1,24],25:[1,25],26:[1,26],27:[1,27],30:[1,10],31:[1,18],33:[1,17],34:[1,15],38:[1,12],39:[1,13],40:[1,14],41:[1,3],42:[1,4],45:[1,5],46:[1,6],47:[1,7],51:[1,16]},{18:[1,74],19:[1,73],20:[1,75],21:[1,77],22:[1,78],24:[1,80],25:[1,81],26:[1,83],27:[1,84],28:71,29:[1,72],30:[1,76],31:[1,79],32:[1,82],33:[1,85],34:[1,86],35:[1,87],36:[1,88],37:[1,89],38:[1,90],39:[1,91],40:[1,92],41:[1,93],42:[1,94]},{18:[1,74],19:[1,73],20:[1,75],21:[1,77],22:[1,78],24:[1,80],25:[1,81],26:[1,83],27:[1,84],28:95,29:[1,72],30:[1,76],31:[1,79],32:[1,82],33:[1,85],34:[1,86],35:[1,87],36:[1,88],37:[1,89],38:[1,90],39:[1,91],40:[1,92],41:[1,93],42:[1,94]},{5:[2,47],43:[2,47],44:[2,47],48:[2,47]},{5:[2,48],43:[2,48],44:[2,48],48:[2,48]},{43:[1,29],44:[1,30],48:[1,96]},{35:[1,97],37:[1,98]},{15:[1,99]},{15:[1,100]},{15:[1,101]},{15:[2,6]},{15:[2,7]},{15:[2,2]},{15:[2,3]},{15:[2,4]},{15:[2,5]},{15:[1,102]},{15:[1,103]},{15:[1,104]},{18:[1,74],19:[1,73],20:[1,75],21:[1,77],22:[1,78],24:[1,80],25:[1,81],26:[1,83],27:[1,84],28:105,29:[1,72],30:[1,76],31:[1,79],32:[1,82],33:[1,85],34:[1,86],35:[1,87],36:[1,88],37:[1,89],38:[1,90],39:[1,91],40:[1,92],41:[1,93],42:[1,94]},{18:[1,74],19:[1,73],20:[1,75],21:[1,77],22:[1,78],24:[1,80],25:[1,81],26:[1,83],27:[1,84],28:106,29:[1,72],30:[1,76],31:[1,79],32:[1,82],33:[1,85],34:[1,86],35:[1,87],36:[1,88],37:[1,89],38:[1,90],39:[1,91],40:[1,92],41:[1,93],42:[1,94]},{18:[1,74],19:[1,73],20:[1,75],21:[1,77],22:[1,78],24:[1,80],25:[1,81],26:[1,83],27:[1,84],28:107,29:[1,72],30:[1,76],31:[1,79],32:[1,82],33:[1,85],34:[1,86],35:[1,87],36:[1,88],37:[1,89],38:[1,90],39:[1,91],40:[1,92],41:[1,93],42:[1,94]},{13:108,14:[1,109],15:[1,110]},{13:111,14:[1,109],15:[1,110]},{13:112,14:[1,109],15:[1,110]},{13:113,14:[1,109],15:[1,110]},{13:114,14:[1,109],15:[1,110]},{13:115,14:[1,109],15:[1,110]},{13:116,14:[1,109],15:[1,110]},{13:117,14:[1,109],15:[1,110]},{18:[1,74],19:[1,73],20:[1,75],21:[1,77],22:[1,78],24:[1,80],25:[1,81],26:[1,83],27:[1,84],28:118,29:[1,72],30:[1,76],31:[1,79],32:[1,82],33:[1,85],34:[1,86],35:[1,87],36:[1,88],37:[1,89],38:[1,90],39:[1,91],40:[1,92],41:[1,93],42:[1,94]},{18:[1,74],19:[1,73],20:[1,75],21:[1,77],22:[1,78],24:[1,80],25:[1,81],26:[1,83],27:[1,84],28:119,29:[1,72],30:[1,76],31:[1,79],32:[1,82],33:[1,85],34:[1,86],35:[1,87],36:[1,88],37:[1,89],38:[1,90],39:[1,91],40:[1,92],41:[1,93],42:[1,94]},{18:[1,74],19:[1,73],20:[1,75],21:[1,77],22:[1,78],24:[1,80],25:[1,81],26:[1,83],27:[1,84],28:120,29:[1,72],30:[1,76],31:[1,79],32:[1,82],33:[1,85],34:[1,86],35:[1,87],36:[1,88],37:[1,89],38:[1,90],39:[1,91],40:[1,92],41:[1,93],42:[1,94]},{18:[1,74],19:[1,73],20:[1,75],21:[1,77],22:[1,78],24:[1,80],25:[1,81],26:[1,83],27:[1,84],28:121,29:[1,72],30:[1,76],31:[1,79],32:[1,82],33:[1,85],34:[1,86],35:[1,87],36:[1,88],37:[1,89],38:[1,90],39:[1,91],40:[1,92],41:[1,93],42:[1,94]},{18:[1,74],19:[1,73],20:[1,75],21:[1,77],22:[1,78],24:[1,80],25:[1,81],26:[1,83],27:[1,84],28:122,29:[1,72],30:[1,76],31:[1,79],32:[1,82],33:[1,85],34:[1,86],35:[1,87],36:[1,88],37:[1,89],38:[1,90],39:[1,91],40:[1,92],41:[1,93],42:[1,94]},{18:[1,74],19:[1,73],20:[1,75],21:[1,77],22:[1,78],24:[1,80],25:[1,81],26:[1,83],27:[1,84],28:123,29:[1,72],30:[1,76],31:[1,79],32:[1,82],33:[1,85],34:[1,86],35:[1,87],36:[1,88],37:[1,89],38:[1,90],39:[1,91],40:[1,92],41:[1,93],42:[1,94]},{29:[1,124]},{29:[1,125]},{29:[1,126]},{5:[2,43],43:[2,43],44:[2,43],48:[2,43]},{5:[2,46],43:[1,29],44:[2,46],48:[2,46]},{5:[2,44],43:[2,44],44:[2,44],48:[2,44]},{5:[2,20],43:[2,20],44:[2,20],48:[2,20]},{5:[2,21],43:[2,21],44:[2,21],48:[2,21]},{5:[2,22],43:[2,22],44:[2,22],48:[2,22]},{5:[2,23],43:[2,23],44:[2,23],48:[2,23]},{5:[2,24],43:[2,24],44:[2,24],48:[2,24]},{5:[2,25],43:[2,25],44:[2,25],48:[2,25]},{5:[2,26],43:[2,26],44:[2,26],48:[2,26]},{5:[2,27],43:[2,27],44:[2,27],48:[2,27]},{5:[2,28],43:[2,28],44:[2,28],48:[2,28]},{5:[2,29],43:[2,29],44:[2,29],48:[2,29]},{5:[2,30],43:[2,30],44:[2,30],48:[2,30]},{5:[2,31],43:[2,31],44:[2,31],48:[2,31]},{5:[2,32],43:[2,32],44:[2,32],48:[2,32]},{5:[2,33],43:[2,33],44:[2,33],48:[2,33]},{5:[2,34],43:[2,34],44:[2,34],48:[2,34]},{5:[2,35],43:[2,35],44:[2,35],48:[2,35]},{5:[2,36],43:[2,36],44:[2,36],48:[2,36]},{5:[2,37],43:[2,37],44:[2,37],48:[2,37]},{5:[2,38],43:[2,38],44:[2,38],48:[2,38]},{5:[2,39],43:[2,39],44:[2,39],48:[2,39]},{5:[2,40],43:[2,40],44:[2,40],48:[2,40]},{5:[2,41],43:[2,41],44:[2,41],48:[2,41]},{5:[2,42],43:[2,42],44:[2,42],48:[2,42]},{5:[2,45],43:[2,45],44:[2,45],48:[2,45]},{5:[2,49],43:[2,49],44:[2,49],48:[2,49]},{5:[2,50],43:[2,50],44:[2,50],48:[2,50]},{5:[2,51],43:[2,51],44:[2,51],48:[2,51]},{5:[2,52],43:[2,52],44:[2,52],48:[2,52]},{5:[2,53],43:[2,53],44:[2,53],48:[2,53]},{5:[2,54],43:[2,54],44:[2,54],48:[2,54]},{5:[2,55],43:[2,55],44:[2,55],48:[2,55]},{5:[2,59],43:[2,59],44:[2,59],48:[2,59]},{5:[2,60],43:[2,60],44:[2,60],48:[2,60]},{5:[2,56],43:[2,56],44:[2,56],48:[2,56]},{5:[2,57],43:[2,57],44:[2,57],48:[2,57]},{5:[2,58],43:[2,58],44:[2,58],48:[2,58]},{5:[2,61],43:[2,61],44:[2,61],48:[2,61]},{5:[2,8],43:[2,8],44:[2,8],48:[2,8]},{5:[2,9],43:[2,9],44:[2,9],48:[2,9]},{5:[2,62],43:[2,62],44:[2,62],48:[2,62]},{5:[2,63],43:[2,63],44:[2,63],48:[2,63]},{5:[2,64],43:[2,64],44:[2,64],48:[2,64]},{5:[2,65],43:[2,65],44:[2,65],48:[2,65]},{5:[2,66],43:[2,66],44:[2,66],48:[2,66]},{5:[2,67],43:[2,67],44:[2,67],48:[2,67]},{5:[2,68],43:[2,68],44:[2,68],48:[2,68]},{5:[2,69],43:[2,69],44:[2,69],48:[2,69]},{5:[2,70],43:[2,70],44:[2,70],48:[2,70]},{5:[2,71],43:[2,71],44:[2,71],48:[2,71]},{5:[2,72],43:[2,72],44:[2,72],48:[2,72]},{5:[2,73],43:[2,73],44:[2,73],48:[2,73]},{5:[2,74],43:[2,74],44:[2,74],48:[2,74]},{5:[2,75],43:[2,75],44:[2,75],48:[2,75]},{5:[2,76],43:[2,76],44:[2,76],48:[2,76]},{5:[2,77],43:[2,77],44:[2,77],48:[2,77]}],
defaultActions: {28:[2,1],40:[2,6],41:[2,7],42:[2,2],43:[2,3],44:[2,4],45:[2,5]},
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
    return {or: [t1, t2]};
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
case 7:return 30
break;
case 8:return 21
break;
case 9:return 22
break;
case 10:return 26
break;
case 11:return 31
break;
case 12:return 24
break;
case 13:return 25
break;
case 14:return "ip"
break;
case 15:return "ip1"
break;
case 16:return "ip2"
break;
case 17:return "uri"
break;
case 18:return "ua"
break;
case 19:return "tcp"
break;
case 20:return "udp"
break;
case 21:return "host"
break;
case 22:return "header"
break;
case 23:return "xff"
break;
case 24:return 36
break;
case 25:return 51
break;
case 26:return 29
break;
case 27:return 32
break;
case 28:return 8
break;
case 29:return 7
break;
case 30:return 11
break;
case 31:return 10
break;
case 32:return 50
break;
case 33:return 49
break;
case 34:return 49
break;
case 35:return 44
break;
case 36:return 44
break;
case 37:return 43
break;
case 38:return 43
break;
case 39:return 47
break;
case 40:return 48
break;
case 41:return 45
break;
case 42:return 5
break;
case 43:return 'INVALID'
break;
case 44:console.log(yy_.yytext);
break;
}
};
lexer.rules = [/^(?:\s+)/,/^(?:[0-9]+\b)/,/^(?:([0-9]{1,3})?(\.[0-9]{1,3})?(\.[0-9]{1,3})?(\.[0-9]{1,3})?(\/[0-9]{1,2})?(:[0-9]{1,5})?\b)/,/^(?:bytes)/,/^(?:databytes)/,/^(?:packets)/,/^(?:protocol)/,/^(?:port)/,/^(?:port1)/,/^(?:port2)/,/^(?:node)/,/^(?:country)/,/^(?:country1)/,/^(?:country2)/,/^(?:ip)/,/^(?:ip1)/,/^(?:ip2)/,/^(?:uri)/,/^(?:ua)/,/^(?:tcp)/,/^(?:udp)/,/^(?:host)/,/^(?:header)/,/^(?:xff)/,/^(?:contains)/,/^(?:tags)/,/^(?:[\w*._:-]+)/,/^(?:"[^"]+")/,/^(?:<=)/,/^(?:<)/,/^(?:>=)/,/^(?:>)/,/^(?:!=)/,/^(?:==)/,/^(?:=)/,/^(?:\|\|)/,/^(?:\|)/,/^(?:&&)/,/^(?:&)/,/^(?:\()/,/^(?:\))/,/^(?:!)/,/^(?:$)/,/^(?:.)/,/^(?:.)/];
lexer.conditions = {"INITIAL":{"rules":[0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44],"inclusive":true}};
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