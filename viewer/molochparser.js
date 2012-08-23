/* Jison generated parser */
var molochparser = (function(){
var parser = {trace: function trace() { },
yy: {},
symbols_: {"error":2,"expressions":3,"e":4,"EOF":5,"LTA":6,"lt":7,"lte":8,"GTA":9,"gt":10,"gte":11,"GTLT":12,"IPNUM":13,"IPMATCH":14,"NUMBER":15,"RANGEFIELD":16,"databytes":17,"bytes":18,"packets":19,"protocol":20,"port.src":21,"port.dst":22,"STRFIELD":23,"country.src":24,"country.dst":25,"country.xff":26,"node":27,"host":28,"STR":29,"ID":30,"port":31,"src":32,"dst":33,"country":34,"xff":35,"QUOTEDSTR":36,"header":37,"tcp":38,"contains":39,"udp":40,"ip":41,"uri":42,"ua":43,"&&":44,"||":45,"!":46,"-":47,"(":48,")":49,"==":50,"!=":51,"ip.src":52,"ip.dst":53,"ip.xff":54,"tags":55,"$accept":0,"$end":1},
terminals_: {2:"error",5:"EOF",7:"lt",8:"lte",10:"gt",11:"gte",14:"IPMATCH",15:"NUMBER",17:"databytes",18:"bytes",19:"packets",20:"protocol",21:"port.src",22:"port.dst",24:"country.src",25:"country.dst",26:"country.xff",27:"node",28:"host",30:"ID",31:"port",32:"src",33:"dst",34:"country",35:"xff",36:"QUOTEDSTR",37:"header",38:"tcp",39:"contains",40:"udp",41:"ip",42:"uri",43:"ua",44:"&&",45:"||",46:"!",47:"-",48:"(",49:")",50:"==",51:"!=",52:"ip.src",53:"ip.dst",54:"ip.xff",55:"tags"},
productions_: [0,[3,2],[6,1],[6,1],[9,1],[9,1],[12,1],[12,1],[13,1],[13,1],[16,1],[16,1],[16,1],[16,1],[16,1],[16,1],[23,1],[23,1],[23,1],[23,1],[23,1],[29,1],[29,1],[29,1],[29,1],[29,1],[29,2],[29,2],[29,1],[29,2],[29,2],[29,2],[29,1],[29,1],[29,1],[29,1],[29,1],[29,1],[29,1],[29,1],[29,2],[29,2],[29,2],[29,1],[29,1],[4,3],[4,3],[4,3],[4,3],[4,2],[4,2],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3]],
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
case 18:this.$ = 'gxff'
break;
case 19:this.$ = 'no'
break;
case 20:this.$ = 'ho'
break;
case 45:this.$ = {and: [$$[$0-2], $$[$0]]};
break;
case 46:this.$ = {query: {text: {us: {query: $$[$0], type: "phrase", operator: "and"}}}};
break;
case 47:this.$ = {query: {text: {ua: {query: $$[$0], type: "phrase", operator: "and"}}}};
break;
case 48:this.$ = {or: [$$[$0-2], $$[$0]]};
break;
case 49:this.$ = {not: $$[$0]};
break;
case 50:this.$ = -$$[$0];
break;
case 51:this.$ = $$[$0-1];
break;
case 52:this.$ = {term: {pr: 6}};
break;
case 53:this.$ = {term: {pr: 17}};
break;
case 54:this.$ = {range: {}};
         this.$.range[$$[$0-2]] = {};
         this.$.range[$$[$0-2]][$$[$0-1]] = $$[$0];
break;
case 55:this.$ = {term: {}};
         this.$.term[$$[$0-2]] = $$[$0];
break;
case 56:this.$ = {not: {term: {}}};
         this.$.not.term[$$[$0-2]] = $$[$0];
break;
case 57:this.$ = {or: [{range: {p1: {}}}, {range: {p2: {}}}]};
         this.$.or[0].range.p1[$$[$0-1]] = $$[$0];
         this.$.or[1].range.p2[$$[$0-1]] = $$[$0];
break;
case 58:this.$ = {term: {}};
         this.$.term[$$[$0-2]] = $$[$0];
break;
case 59:this.$ = {not: {term: {}}};
         this.$.not.term[$$[$0-2]] = $$[$0];
break;
case 60: var str = stripQuotes($$[$0]);
          if (str.indexOf("*") !== -1) {
            this.$ = {query: {wildcard: {}}};
            this.$.query.wildcard[$$[$0-2]] = str;
          } else {
            this.$ = {term: {}};
            this.$.term[$$[$0-2]] = str;
          }
        
break;
case 61:this.$ = {or: [{term: {p1: $$[$0]}}, {term: {p2: $$[$0]}}]};
break;
case 62:this.$ = {not: {or: [{term: {p1: $$[$0]}}, {term: {p2: $$[$0]}}]}};
break;
case 63:this.$ = parseIpPort($$[$0],0);
break;
case 64:this.$ = {not: parseIpPort($$[$0],0)};
break;
case 65:this.$ = parseIpPort($$[$0],1);
break;
case 66:this.$ = {not: parseIpPort($$[$0],1)};
break;
case 67:this.$ = parseIpPort($$[$0],2);
break;
case 68:this.$ = {not: parseIpPort($$[$0],2)};
break;
case 69:this.$ = parseIpPort($$[$0],3);
break;
case 70:this.$ = {not: parseIpPort($$[$0],3)};
break;
case 71: var tag = stripQuotes($$[$0]);
          this.$ = {term: {ta: tag}};
        
break;
case 72: var tag = stripQuotes($$[$0]);
          this.$ = {not: {term: {ta: tag}}};
        
break;
case 73: var tag = stripQuotes($$[$0]);
          this.$ = {term: {ta: tag}};
        
break;
case 74: var tag = stripQuotes($$[$0]);
          this.$ = {term: {hh: tag}};
        
break;
case 75: var tag = stripQuotes($$[$0]);
          this.$ = {not: {term: {hh: tag}}};
        
break;
case 76: var tag = stripQuotes($$[$0]);
          this.$ = {term: {hh: tag}};
        
break;
case 77:this.$ = {or: [{term: {g1: $$[$0].toUpperCase()}}, {term: {g2: $$[$0].toUpperCase()}}, {term: {gxff: $$[$0].toUpperCase()}}]};
break;
case 78:this.$ = {not: {or: [{term: {g1: $$[$0].toUpperCase()}}, {term: {g2: $$[$0].toUpperCase()}}, {term: {gxff: $$[$0].toUpperCase()}}]}};
break;
case 79:this.$ = {or: [{query: {wildcard: {g1: $$[$0].toUpperCase()}}}, {query: {wildcard: {g2: $$[$0].toUpperCase()}}}, {query: {wildcard: {gxff: $$[$0].toUpperCase()}}}]};
break;
case 80: var str = stripQuotes($$[$0]);
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
table: [{3:1,4:2,16:9,17:[1,19],18:[1,20],19:[1,21],20:[1,8],21:[1,22],22:[1,23],23:11,24:[1,24],25:[1,25],26:[1,26],27:[1,27],28:[1,28],31:[1,10],34:[1,18],37:[1,17],41:[1,12],42:[1,3],43:[1,4],46:[1,5],47:[1,6],48:[1,7],52:[1,13],53:[1,14],54:[1,15],55:[1,16]},{1:[3]},{5:[1,29],44:[1,30],45:[1,31]},{39:[1,32]},{39:[1,33]},{4:34,16:9,17:[1,19],18:[1,20],19:[1,21],20:[1,8],21:[1,22],22:[1,23],23:11,24:[1,24],25:[1,25],26:[1,26],27:[1,27],28:[1,28],31:[1,10],34:[1,18],37:[1,17],41:[1,12],42:[1,3],43:[1,4],46:[1,5],47:[1,6],48:[1,7],52:[1,13],53:[1,14],54:[1,15],55:[1,16]},{4:35,16:9,17:[1,19],18:[1,20],19:[1,21],20:[1,8],21:[1,22],22:[1,23],23:11,24:[1,24],25:[1,25],26:[1,26],27:[1,27],28:[1,28],31:[1,10],34:[1,18],37:[1,17],41:[1,12],42:[1,3],43:[1,4],46:[1,5],47:[1,6],48:[1,7],52:[1,13],53:[1,14],54:[1,15],55:[1,16]},{4:36,16:9,17:[1,19],18:[1,20],19:[1,21],20:[1,8],21:[1,22],22:[1,23],23:11,24:[1,24],25:[1,25],26:[1,26],27:[1,27],28:[1,28],31:[1,10],34:[1,18],37:[1,17],41:[1,12],42:[1,3],43:[1,4],46:[1,5],47:[1,6],48:[1,7],52:[1,13],53:[1,14],54:[1,15],55:[1,16]},{7:[2,13],8:[2,13],10:[2,13],11:[2,13],50:[1,37],51:[2,13]},{6:41,7:[1,43],8:[1,44],9:42,10:[1,45],11:[1,46],12:38,50:[1,39],51:[1,40]},{6:41,7:[1,43],8:[1,44],9:42,10:[1,45],11:[1,46],12:47,50:[1,48],51:[1,49]},{39:[1,52],50:[1,50],51:[1,51]},{50:[1,53],51:[1,54]},{50:[1,55],51:[1,56]},{50:[1,57],51:[1,58]},{50:[1,59],51:[1,60]},{39:[1,63],50:[1,61],51:[1,62]},{39:[1,66],50:[1,64],51:[1,65]},{39:[1,69],50:[1,67],51:[1,68]},{7:[2,10],8:[2,10],10:[2,10],11:[2,10],50:[2,10],51:[2,10]},{7:[2,11],8:[2,11],10:[2,11],11:[2,11],50:[2,11],51:[2,11]},{7:[2,12],8:[2,12],10:[2,12],11:[2,12],50:[2,12],51:[2,12]},{7:[2,14],8:[2,14],10:[2,14],11:[2,14],50:[2,14],51:[2,14]},{7:[2,15],8:[2,15],10:[2,15],11:[2,15],50:[2,15],51:[2,15]},{39:[2,16],50:[2,16],51:[2,16]},{39:[2,17],50:[2,17],51:[2,17]},{39:[2,18],50:[2,18],51:[2,18]},{39:[2,19],50:[2,19],51:[2,19]},{39:[2,20],50:[2,20],51:[2,20]},{1:[2,1]},{4:70,16:9,17:[1,19],18:[1,20],19:[1,21],20:[1,8],21:[1,22],22:[1,23],23:11,24:[1,24],25:[1,25],26:[1,26],27:[1,27],28:[1,28],31:[1,10],34:[1,18],37:[1,17],41:[1,12],42:[1,3],43:[1,4],46:[1,5],47:[1,6],48:[1,7],52:[1,13],53:[1,14],54:[1,15],55:[1,16]},{4:71,16:9,17:[1,19],18:[1,20],19:[1,21],20:[1,8],21:[1,22],22:[1,23],23:11,24:[1,24],25:[1,25],26:[1,26],27:[1,27],28:[1,28],31:[1,10],34:[1,18],37:[1,17],41:[1,12],42:[1,3],43:[1,4],46:[1,5],47:[1,6],48:[1,7],52:[1,13],53:[1,14],54:[1,15],55:[1,16]},{18:[1,75],19:[1,74],20:[1,76],27:[1,80],28:[1,81],29:72,30:[1,73],31:[1,77],34:[1,78],36:[1,79],37:[1,82],38:[1,83],39:[1,84],40:[1,85],41:[1,86],42:[1,87],43:[1,88]},{18:[1,75],19:[1,74],20:[1,76],27:[1,80],28:[1,81],29:89,30:[1,73],31:[1,77],34:[1,78],36:[1,79],37:[1,82],38:[1,83],39:[1,84],40:[1,85],41:[1,86],42:[1,87],43:[1,88]},{5:[2,49],44:[2,49],45:[2,49],49:[2,49]},{5:[2,50],44:[2,50],45:[2,50],49:[2,50]},{44:[1,30],45:[1,31],49:[1,90]},{38:[1,91],40:[1,92]},{15:[1,93]},{15:[1,94]},{15:[1,95]},{15:[2,6]},{15:[2,7]},{15:[2,2]},{15:[2,3]},{15:[2,4]},{15:[2,5]},{15:[1,96]},{15:[1,97]},{15:[1,98]},{18:[1,75],19:[1,74],20:[1,76],27:[1,80],28:[1,81],29:99,30:[1,73],31:[1,77],34:[1,78],36:[1,79],37:[1,82],38:[1,83],39:[1,84],40:[1,85],41:[1,86],42:[1,87],43:[1,88]},{18:[1,75],19:[1,74],20:[1,76],27:[1,80],28:[1,81],29:100,30:[1,73],31:[1,77],34:[1,78],36:[1,79],37:[1,82],38:[1,83],39:[1,84],40:[1,85],41:[1,86],42:[1,87],43:[1,88]},{18:[1,75],19:[1,74],20:[1,76],27:[1,80],28:[1,81],29:101,30:[1,73],31:[1,77],34:[1,78],36:[1,79],37:[1,82],38:[1,83],39:[1,84],40:[1,85],41:[1,86],42:[1,87],43:[1,88]},{13:102,14:[1,103],15:[1,104]},{13:105,14:[1,103],15:[1,104]},{13:106,14:[1,103],15:[1,104]},{13:107,14:[1,103],15:[1,104]},{13:108,14:[1,103],15:[1,104]},{13:109,14:[1,103],15:[1,104]},{13:110,14:[1,103],15:[1,104]},{13:111,14:[1,103],15:[1,104]},{18:[1,75],19:[1,74],20:[1,76],27:[1,80],28:[1,81],29:112,30:[1,73],31:[1,77],34:[1,78],36:[1,79],37:[1,82],38:[1,83],39:[1,84],40:[1,85],41:[1,86],42:[1,87],43:[1,88]},{18:[1,75],19:[1,74],20:[1,76],27:[1,80],28:[1,81],29:113,30:[1,73],31:[1,77],34:[1,78],36:[1,79],37:[1,82],38:[1,83],39:[1,84],40:[1,85],41:[1,86],42:[1,87],43:[1,88]},{18:[1,75],19:[1,74],20:[1,76],27:[1,80],28:[1,81],29:114,30:[1,73],31:[1,77],34:[1,78],36:[1,79],37:[1,82],38:[1,83],39:[1,84],40:[1,85],41:[1,86],42:[1,87],43:[1,88]},{18:[1,75],19:[1,74],20:[1,76],27:[1,80],28:[1,81],29:115,30:[1,73],31:[1,77],34:[1,78],36:[1,79],37:[1,82],38:[1,83],39:[1,84],40:[1,85],41:[1,86],42:[1,87],43:[1,88]},{18:[1,75],19:[1,74],20:[1,76],27:[1,80],28:[1,81],29:116,30:[1,73],31:[1,77],34:[1,78],36:[1,79],37:[1,82],38:[1,83],39:[1,84],40:[1,85],41:[1,86],42:[1,87],43:[1,88]},{18:[1,75],19:[1,74],20:[1,76],27:[1,80],28:[1,81],29:117,30:[1,73],31:[1,77],34:[1,78],36:[1,79],37:[1,82],38:[1,83],39:[1,84],40:[1,85],41:[1,86],42:[1,87],43:[1,88]},{30:[1,118]},{30:[1,119]},{30:[1,120]},{5:[2,45],44:[2,45],45:[2,45],49:[2,45]},{5:[2,48],44:[1,30],45:[2,48],49:[2,48]},{5:[2,46],44:[2,46],45:[2,46],49:[2,46]},{5:[2,21],44:[2,21],45:[2,21],49:[2,21]},{5:[2,22],44:[2,22],45:[2,22],49:[2,22]},{5:[2,23],44:[2,23],45:[2,23],49:[2,23]},{5:[2,24],44:[2,24],45:[2,24],49:[2,24]},{5:[2,25],32:[1,121],33:[1,122],44:[2,25],45:[2,25],49:[2,25]},{5:[2,28],32:[1,123],33:[1,124],35:[1,125],44:[2,28],45:[2,28],49:[2,28]},{5:[2,32],44:[2,32],45:[2,32],49:[2,32]},{5:[2,33],44:[2,33],45:[2,33],49:[2,33]},{5:[2,34],44:[2,34],45:[2,34],49:[2,34]},{5:[2,35],44:[2,35],45:[2,35],49:[2,35]},{5:[2,36],44:[2,36],45:[2,36],49:[2,36]},{5:[2,37],44:[2,37],45:[2,37],49:[2,37]},{5:[2,38],44:[2,38],45:[2,38],49:[2,38]},{5:[2,39],32:[1,126],33:[1,127],35:[1,128],44:[2,39],45:[2,39],49:[2,39]},{5:[2,43],44:[2,43],45:[2,43],49:[2,43]},{5:[2,44],44:[2,44],45:[2,44],49:[2,44]},{5:[2,47],44:[2,47],45:[2,47],49:[2,47]},{5:[2,51],44:[2,51],45:[2,51],49:[2,51]},{5:[2,52],44:[2,52],45:[2,52],49:[2,52]},{5:[2,53],44:[2,53],45:[2,53],49:[2,53]},{5:[2,54],44:[2,54],45:[2,54],49:[2,54]},{5:[2,55],44:[2,55],45:[2,55],49:[2,55]},{5:[2,56],44:[2,56],45:[2,56],49:[2,56]},{5:[2,57],44:[2,57],45:[2,57],49:[2,57]},{5:[2,61],44:[2,61],45:[2,61],49:[2,61]},{5:[2,62],44:[2,62],45:[2,62],49:[2,62]},{5:[2,58],44:[2,58],45:[2,58],49:[2,58]},{5:[2,59],44:[2,59],45:[2,59],49:[2,59]},{5:[2,60],44:[2,60],45:[2,60],49:[2,60]},{5:[2,63],44:[2,63],45:[2,63],49:[2,63]},{5:[2,8],44:[2,8],45:[2,8],49:[2,8]},{5:[2,9],44:[2,9],45:[2,9],49:[2,9]},{5:[2,64],44:[2,64],45:[2,64],49:[2,64]},{5:[2,65],44:[2,65],45:[2,65],49:[2,65]},{5:[2,66],44:[2,66],45:[2,66],49:[2,66]},{5:[2,67],44:[2,67],45:[2,67],49:[2,67]},{5:[2,68],44:[2,68],45:[2,68],49:[2,68]},{5:[2,69],44:[2,69],45:[2,69],49:[2,69]},{5:[2,70],44:[2,70],45:[2,70],49:[2,70]},{5:[2,71],44:[2,71],45:[2,71],49:[2,71]},{5:[2,72],44:[2,72],45:[2,72],49:[2,72]},{5:[2,73],44:[2,73],45:[2,73],49:[2,73]},{5:[2,74],44:[2,74],45:[2,74],49:[2,74]},{5:[2,75],44:[2,75],45:[2,75],49:[2,75]},{5:[2,76],44:[2,76],45:[2,76],49:[2,76]},{5:[2,77],44:[2,77],45:[2,77],49:[2,77]},{5:[2,78],44:[2,78],45:[2,78],49:[2,78]},{5:[2,79],44:[2,79],45:[2,79],49:[2,79]},{5:[2,26],44:[2,26],45:[2,26],49:[2,26]},{5:[2,27],44:[2,27],45:[2,27],49:[2,27]},{5:[2,29],44:[2,29],45:[2,29],49:[2,29]},{5:[2,30],44:[2,30],45:[2,30],49:[2,30]},{5:[2,31],44:[2,31],45:[2,31],49:[2,31]},{5:[2,40],44:[2,40],45:[2,40],49:[2,40]},{5:[2,41],44:[2,41],45:[2,41],49:[2,41]},{5:[2,42],44:[2,42],45:[2,42],49:[2,42]}],
defaultActions: {29:[2,1],41:[2,6],42:[2,7],43:[2,2],44:[2,3],45:[2,4],46:[2,5]},
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
case 9:return 31
break;
case 10:return 27
break;
case 11:return 24
break;
case 12:return 25
break;
case 13:return 26
break;
case 14:return 34
break;
case 15:return "ip.src"
break;
case 16:return "ip.dst"
break;
case 17:return "ip.xff"
break;
case 18:return "ip"
break;
case 19:return "uri"
break;
case 20:return "ua"
break;
case 21:return "tcp"
break;
case 22:return "udp"
break;
case 23:return "host"
break;
case 24:return "header"
break;
case 25:return 39
break;
case 26:return 55
break;
case 27:return 30
break;
case 28:return 36
break;
case 29:return 8
break;
case 30:return 7
break;
case 31:return 11
break;
case 32:return 10
break;
case 33:return 51
break;
case 34:return 50
break;
case 35:return 50
break;
case 36:return 45
break;
case 37:return 45
break;
case 38:return 44
break;
case 39:return 44
break;
case 40:return 48
break;
case 41:return 49
break;
case 42:return 46
break;
case 43:return 5
break;
case 44:return 'INVALID'
break;
case 45:console.log(yy_.yytext);
break;
}
};
lexer.rules = [/^(?:\s+)/,/^(?:[0-9]+\b)/,/^(?:([0-9]{1,3})?(\.[0-9]{1,3})?(\.[0-9]{1,3})?(\.[0-9]{1,3})?(\/[0-9]{1,2})?(:[0-9]{1,5})?\b)/,/^(?:bytes)/,/^(?:databytes)/,/^(?:packets)/,/^(?:protocol)/,/^(?:port\.src)/,/^(?:port\.dst)/,/^(?:port)/,/^(?:node)/,/^(?:country\.src)/,/^(?:country\.dst)/,/^(?:country\.xff)/,/^(?:country)/,/^(?:ip\.src)/,/^(?:ip\.dst)/,/^(?:ip\.xff)/,/^(?:ip)/,/^(?:uri)/,/^(?:ua)/,/^(?:tcp)/,/^(?:udp)/,/^(?:host)/,/^(?:header)/,/^(?:contains)/,/^(?:tags)/,/^(?:[\w*._:-]+)/,/^(?:"[^"]+")/,/^(?:<=)/,/^(?:<)/,/^(?:>=)/,/^(?:>)/,/^(?:!=)/,/^(?:==)/,/^(?:=)/,/^(?:\|\|)/,/^(?:\|)/,/^(?:&&)/,/^(?:&)/,/^(?:\()/,/^(?:\))/,/^(?:!)/,/^(?:$)/,/^(?:.)/,/^(?:.)/];
lexer.conditions = {"INITIAL":{"rules":[0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45],"inclusive":true}};
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