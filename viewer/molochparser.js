/* Jison generated parser */
var molochparser = (function(){
var parser = {trace: function trace() { },
yy: {},
symbols_: {"error":2,"expressions":3,"e":4,"EOF":5,"LTA":6,"lt":7,"lte":8,"GTA":9,"gt":10,"gte":11,"GTLT":12,"IPNUM":13,"IPMATCH":14,"NUMBER":15,"RANGEFIELD":16,"databytes":17,"bytes":18,"packets":19,"protocol":20,"port1":21,"port2":22,"STRFIELD":23,"country1":24,"country2":25,"node":26,"host":27,"STR":28,"ID":29,"port":30,"country":31,"QUOTEDSTR":32,"tcp":33,"contains":34,"udp":35,"ip":36,"ip1":37,"ip2":38,"uri":39,"ua":40,"&&":41,"||":42,"!":43,"-":44,"(":45,")":46,"==":47,"!=":48,"tags":49,"$accept":0,"$end":1},
terminals_: {2:"error",5:"EOF",7:"lt",8:"lte",10:"gt",11:"gte",14:"IPMATCH",15:"NUMBER",17:"databytes",18:"bytes",19:"packets",20:"protocol",21:"port1",22:"port2",24:"country1",25:"country2",26:"node",27:"host",29:"ID",30:"port",31:"country",32:"QUOTEDSTR",33:"tcp",34:"contains",35:"udp",36:"ip",37:"ip1",38:"ip2",39:"uri",40:"ua",41:"&&",42:"||",43:"!",44:"-",45:"(",46:")",47:"==",48:"!=",49:"tags"},
productions_: [0,[3,2],[6,1],[6,1],[9,1],[9,1],[12,1],[12,1],[13,1],[13,1],[16,1],[16,1],[16,1],[16,1],[16,1],[16,1],[23,1],[23,1],[23,1],[23,1],[28,1],[28,1],[28,1],[28,1],[28,1],[28,1],[28,1],[28,1],[28,1],[28,1],[28,1],[28,1],[28,1],[28,1],[28,1],[28,1],[28,1],[28,1],[28,1],[28,1],[28,1],[4,3],[4,3],[4,3],[4,3],[4,2],[4,2],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3],[4,3]],
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
case 41:this.$ = {and: [$$[$0-2], $$[$0]]};
break;
case 42:this.$ = {query: {text: {us: {query: $$[$0], type: "phrase", operator: "and"}}}};
break;
case 43:this.$ = {query: {text: {ua: {query: $$[$0], type: "phrase", operator: "and"}}}};
break;
case 44:this.$ = {or: [$$[$0-2], $$[$0]]};
break;
case 45:this.$ = {not: $$[$0]};
break;
case 46:this.$ = -$$[$0];
break;
case 47:this.$ = $$[$0-1];
break;
case 48:this.$ = {term: {pr: 6}};
break;
case 49:this.$ = {term: {pr: 17}};
break;
case 50:this.$ = {numeric_range: {}};
         this.$.numeric_range[$$[$0-2]] = {};
         this.$.numeric_range[$$[$0-2]][$$[$0-1]] = $$[$0];
break;
case 51:this.$ = {term: {}};
         this.$.term[$$[$0-2]] = $$[$0];
break;
case 52:this.$ = {not: {term: {}}};
         this.$.not.term[$$[$0-2]] = $$[$0];
break;
case 53:this.$ = {or: [{numeric_range: {p1: {}}}, {numeric_range: {p2: {}}}]};
         this.$.or[0].numeric_range.p1[$$[$0-1]] = $$[$0];
         this.$.or[1].numeric_range.p2[$$[$0-1]] = $$[$0];
break;
case 54:this.$ = {term: {}};
         this.$.term[$$[$0-2]] = $$[$0];
break;
case 55:this.$ = {not: {term: {}}};
         this.$.not.term[$$[$0-2]] = $$[$0];
break;
case 56: var str = stripQuotes($$[$0]);
          if (str.indexOf("*") !== -1) {
            this.$ = {query: {wildcard: {}}};
            this.$.query.wildcard[$$[$0-2]] = str;
          } else {
            this.$ = {term: {}};
            this.$.term[$$[$0-2]] = str;
          }
        
break;
case 57:this.$ = {or: [{term: {p1: $$[$0]}}, {term: {p2: $$[$0]}}]};
break;
case 58:this.$ = {not: {or: [{term: {p1: $$[$0]}}, {term: {p2: $$[$0]}}]}};
break;
case 59:this.$ = parseIpPort($$[$0],0);
break;
case 60:this.$ = {not: parseIpPort($$[$0],0)};
break;
case 61:this.$ = parseIpPort($$[$0],1);
break;
case 62:this.$ = {not: parseIpPort($$[$0],1)};
break;
case 63:this.$ = parseIpPort($$[$0],2);
break;
case 64:this.$ = {not: parseIpPort($$[$0],2)};
break;
case 65: var tag = stripQuotes($$[$0]);
          this.$ = {term: {ta: tag}};
        
break;
case 66: var tag = stripQuotes($$[$0]);
          this.$ = {not: {term: {ta: tag}}};
        
break;
case 67: var tag = stripQuotes($$[$0]);
          this.$ = {term: {ta: tag}};
        
break;
case 68:this.$ = {or: [{term: {g1: $$[$0].toUpperCase()}}, {term: {g2: $$[$0].toUpperCase()}}]};
break;
case 69:this.$ = {not: {or: [{term: {g1: $$[$0].toUpperCase()}}, {term: {g2: $$[$0].toUpperCase()}}]}};
break;
case 70:this.$ = {or: [{query: {wildcard: {g1: $$[$0].toUpperCase()}}}, {query: {wildcard: {g2: $$[$0].toUpperCase()}}}]};
break;
case 71: var str = stripQuotes($$[$0]);
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
table: [{3:1,4:2,16:9,17:[1,17],18:[1,18],19:[1,19],20:[1,8],21:[1,20],22:[1,21],23:11,24:[1,22],25:[1,23],26:[1,24],27:[1,25],30:[1,10],31:[1,16],36:[1,12],37:[1,13],38:[1,14],39:[1,3],40:[1,4],43:[1,5],44:[1,6],45:[1,7],49:[1,15]},{1:[3]},{5:[1,26],41:[1,27],42:[1,28]},{34:[1,29]},{34:[1,30]},{4:31,16:9,17:[1,17],18:[1,18],19:[1,19],20:[1,8],21:[1,20],22:[1,21],23:11,24:[1,22],25:[1,23],26:[1,24],27:[1,25],30:[1,10],31:[1,16],36:[1,12],37:[1,13],38:[1,14],39:[1,3],40:[1,4],43:[1,5],44:[1,6],45:[1,7],49:[1,15]},{4:32,16:9,17:[1,17],18:[1,18],19:[1,19],20:[1,8],21:[1,20],22:[1,21],23:11,24:[1,22],25:[1,23],26:[1,24],27:[1,25],30:[1,10],31:[1,16],36:[1,12],37:[1,13],38:[1,14],39:[1,3],40:[1,4],43:[1,5],44:[1,6],45:[1,7],49:[1,15]},{4:33,16:9,17:[1,17],18:[1,18],19:[1,19],20:[1,8],21:[1,20],22:[1,21],23:11,24:[1,22],25:[1,23],26:[1,24],27:[1,25],30:[1,10],31:[1,16],36:[1,12],37:[1,13],38:[1,14],39:[1,3],40:[1,4],43:[1,5],44:[1,6],45:[1,7],49:[1,15]},{7:[2,13],8:[2,13],10:[2,13],11:[2,13],47:[1,34],48:[2,13]},{6:38,7:[1,40],8:[1,41],9:39,10:[1,42],11:[1,43],12:35,47:[1,36],48:[1,37]},{6:38,7:[1,40],8:[1,41],9:39,10:[1,42],11:[1,43],12:44,47:[1,45],48:[1,46]},{34:[1,49],47:[1,47],48:[1,48]},{47:[1,50],48:[1,51]},{47:[1,52],48:[1,53]},{47:[1,54],48:[1,55]},{34:[1,58],47:[1,56],48:[1,57]},{34:[1,61],47:[1,59],48:[1,60]},{7:[2,10],8:[2,10],10:[2,10],11:[2,10],47:[2,10],48:[2,10]},{7:[2,11],8:[2,11],10:[2,11],11:[2,11],47:[2,11],48:[2,11]},{7:[2,12],8:[2,12],10:[2,12],11:[2,12],47:[2,12],48:[2,12]},{7:[2,14],8:[2,14],10:[2,14],11:[2,14],47:[2,14],48:[2,14]},{7:[2,15],8:[2,15],10:[2,15],11:[2,15],47:[2,15],48:[2,15]},{34:[2,16],47:[2,16],48:[2,16]},{34:[2,17],47:[2,17],48:[2,17]},{34:[2,18],47:[2,18],48:[2,18]},{34:[2,19],47:[2,19],48:[2,19]},{1:[2,1]},{4:62,16:9,17:[1,17],18:[1,18],19:[1,19],20:[1,8],21:[1,20],22:[1,21],23:11,24:[1,22],25:[1,23],26:[1,24],27:[1,25],30:[1,10],31:[1,16],36:[1,12],37:[1,13],38:[1,14],39:[1,3],40:[1,4],43:[1,5],44:[1,6],45:[1,7],49:[1,15]},{4:63,16:9,17:[1,17],18:[1,18],19:[1,19],20:[1,8],21:[1,20],22:[1,21],23:11,24:[1,22],25:[1,23],26:[1,24],27:[1,25],30:[1,10],31:[1,16],36:[1,12],37:[1,13],38:[1,14],39:[1,3],40:[1,4],43:[1,5],44:[1,6],45:[1,7],49:[1,15]},{18:[1,67],19:[1,66],20:[1,68],21:[1,70],22:[1,71],24:[1,73],25:[1,74],26:[1,76],27:[1,77],28:64,29:[1,65],30:[1,69],31:[1,72],32:[1,75],33:[1,78],34:[1,79],35:[1,80],36:[1,81],37:[1,82],38:[1,83],39:[1,84],40:[1,85]},{18:[1,67],19:[1,66],20:[1,68],21:[1,70],22:[1,71],24:[1,73],25:[1,74],26:[1,76],27:[1,77],28:86,29:[1,65],30:[1,69],31:[1,72],32:[1,75],33:[1,78],34:[1,79],35:[1,80],36:[1,81],37:[1,82],38:[1,83],39:[1,84],40:[1,85]},{5:[2,45],41:[2,45],42:[2,45],46:[2,45]},{5:[2,46],41:[2,46],42:[2,46],46:[2,46]},{41:[1,27],42:[1,28],46:[1,87]},{33:[1,88],35:[1,89]},{15:[1,90]},{15:[1,91]},{15:[1,92]},{15:[2,6]},{15:[2,7]},{15:[2,2]},{15:[2,3]},{15:[2,4]},{15:[2,5]},{15:[1,93]},{15:[1,94]},{15:[1,95]},{18:[1,67],19:[1,66],20:[1,68],21:[1,70],22:[1,71],24:[1,73],25:[1,74],26:[1,76],27:[1,77],28:96,29:[1,65],30:[1,69],31:[1,72],32:[1,75],33:[1,78],34:[1,79],35:[1,80],36:[1,81],37:[1,82],38:[1,83],39:[1,84],40:[1,85]},{18:[1,67],19:[1,66],20:[1,68],21:[1,70],22:[1,71],24:[1,73],25:[1,74],26:[1,76],27:[1,77],28:97,29:[1,65],30:[1,69],31:[1,72],32:[1,75],33:[1,78],34:[1,79],35:[1,80],36:[1,81],37:[1,82],38:[1,83],39:[1,84],40:[1,85]},{18:[1,67],19:[1,66],20:[1,68],21:[1,70],22:[1,71],24:[1,73],25:[1,74],26:[1,76],27:[1,77],28:98,29:[1,65],30:[1,69],31:[1,72],32:[1,75],33:[1,78],34:[1,79],35:[1,80],36:[1,81],37:[1,82],38:[1,83],39:[1,84],40:[1,85]},{13:99,14:[1,100],15:[1,101]},{13:102,14:[1,100],15:[1,101]},{13:103,14:[1,100],15:[1,101]},{13:104,14:[1,100],15:[1,101]},{13:105,14:[1,100],15:[1,101]},{13:106,14:[1,100],15:[1,101]},{18:[1,67],19:[1,66],20:[1,68],21:[1,70],22:[1,71],24:[1,73],25:[1,74],26:[1,76],27:[1,77],28:107,29:[1,65],30:[1,69],31:[1,72],32:[1,75],33:[1,78],34:[1,79],35:[1,80],36:[1,81],37:[1,82],38:[1,83],39:[1,84],40:[1,85]},{18:[1,67],19:[1,66],20:[1,68],21:[1,70],22:[1,71],24:[1,73],25:[1,74],26:[1,76],27:[1,77],28:108,29:[1,65],30:[1,69],31:[1,72],32:[1,75],33:[1,78],34:[1,79],35:[1,80],36:[1,81],37:[1,82],38:[1,83],39:[1,84],40:[1,85]},{18:[1,67],19:[1,66],20:[1,68],21:[1,70],22:[1,71],24:[1,73],25:[1,74],26:[1,76],27:[1,77],28:109,29:[1,65],30:[1,69],31:[1,72],32:[1,75],33:[1,78],34:[1,79],35:[1,80],36:[1,81],37:[1,82],38:[1,83],39:[1,84],40:[1,85]},{29:[1,110]},{29:[1,111]},{29:[1,112]},{5:[2,41],41:[2,41],42:[2,41],46:[2,41]},{5:[2,44],41:[1,27],42:[2,44],46:[2,44]},{5:[2,42],41:[2,42],42:[2,42],46:[2,42]},{5:[2,20],41:[2,20],42:[2,20],46:[2,20]},{5:[2,21],41:[2,21],42:[2,21],46:[2,21]},{5:[2,22],41:[2,22],42:[2,22],46:[2,22]},{5:[2,23],41:[2,23],42:[2,23],46:[2,23]},{5:[2,24],41:[2,24],42:[2,24],46:[2,24]},{5:[2,25],41:[2,25],42:[2,25],46:[2,25]},{5:[2,26],41:[2,26],42:[2,26],46:[2,26]},{5:[2,27],41:[2,27],42:[2,27],46:[2,27]},{5:[2,28],41:[2,28],42:[2,28],46:[2,28]},{5:[2,29],41:[2,29],42:[2,29],46:[2,29]},{5:[2,30],41:[2,30],42:[2,30],46:[2,30]},{5:[2,31],41:[2,31],42:[2,31],46:[2,31]},{5:[2,32],41:[2,32],42:[2,32],46:[2,32]},{5:[2,33],41:[2,33],42:[2,33],46:[2,33]},{5:[2,34],41:[2,34],42:[2,34],46:[2,34]},{5:[2,35],41:[2,35],42:[2,35],46:[2,35]},{5:[2,36],41:[2,36],42:[2,36],46:[2,36]},{5:[2,37],41:[2,37],42:[2,37],46:[2,37]},{5:[2,38],41:[2,38],42:[2,38],46:[2,38]},{5:[2,39],41:[2,39],42:[2,39],46:[2,39]},{5:[2,40],41:[2,40],42:[2,40],46:[2,40]},{5:[2,43],41:[2,43],42:[2,43],46:[2,43]},{5:[2,47],41:[2,47],42:[2,47],46:[2,47]},{5:[2,48],41:[2,48],42:[2,48],46:[2,48]},{5:[2,49],41:[2,49],42:[2,49],46:[2,49]},{5:[2,50],41:[2,50],42:[2,50],46:[2,50]},{5:[2,51],41:[2,51],42:[2,51],46:[2,51]},{5:[2,52],41:[2,52],42:[2,52],46:[2,52]},{5:[2,53],41:[2,53],42:[2,53],46:[2,53]},{5:[2,57],41:[2,57],42:[2,57],46:[2,57]},{5:[2,58],41:[2,58],42:[2,58],46:[2,58]},{5:[2,54],41:[2,54],42:[2,54],46:[2,54]},{5:[2,55],41:[2,55],42:[2,55],46:[2,55]},{5:[2,56],41:[2,56],42:[2,56],46:[2,56]},{5:[2,59],41:[2,59],42:[2,59],46:[2,59]},{5:[2,8],41:[2,8],42:[2,8],46:[2,8]},{5:[2,9],41:[2,9],42:[2,9],46:[2,9]},{5:[2,60],41:[2,60],42:[2,60],46:[2,60]},{5:[2,61],41:[2,61],42:[2,61],46:[2,61]},{5:[2,62],41:[2,62],42:[2,62],46:[2,62]},{5:[2,63],41:[2,63],42:[2,63],46:[2,63]},{5:[2,64],41:[2,64],42:[2,64],46:[2,64]},{5:[2,65],41:[2,65],42:[2,65],46:[2,65]},{5:[2,66],41:[2,66],42:[2,66],46:[2,66]},{5:[2,67],41:[2,67],42:[2,67],46:[2,67]},{5:[2,68],41:[2,68],42:[2,68],46:[2,68]},{5:[2,69],41:[2,69],42:[2,69],46:[2,69]},{5:[2,70],41:[2,70],42:[2,70],46:[2,70]}],
defaultActions: {26:[2,1],38:[2,6],39:[2,7],40:[2,2],41:[2,3],42:[2,4],43:[2,5]},
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
case 22:return 34
break;
case 23:return 49
break;
case 24:return 29
break;
case 25:return 32
break;
case 26:return 8
break;
case 27:return 7
break;
case 28:return 11
break;
case 29:return 10
break;
case 30:return 48
break;
case 31:return 47
break;
case 32:return 47
break;
case 33:return 42
break;
case 34:return 42
break;
case 35:return 41
break;
case 36:return 41
break;
case 37:return 45
break;
case 38:return 46
break;
case 39:return 43
break;
case 40:return 5
break;
case 41:return 'INVALID'
break;
case 42:console.log(yy_.yytext);
break;
}
};
lexer.rules = [/^(?:\s+)/,/^(?:[0-9]+\b)/,/^(?:([0-9]{1,3})?(\.[0-9]{1,3})?(\.[0-9]{1,3})?(\.[0-9]{1,3})?(\/[0-9]{1,2})?(:[0-9]{1,5})?\b)/,/^(?:bytes)/,/^(?:databytes)/,/^(?:packets)/,/^(?:protocol)/,/^(?:port)/,/^(?:port1)/,/^(?:port2)/,/^(?:node)/,/^(?:country)/,/^(?:country1)/,/^(?:country2)/,/^(?:ip)/,/^(?:ip1)/,/^(?:ip2)/,/^(?:uri)/,/^(?:ua)/,/^(?:tcp)/,/^(?:udp)/,/^(?:host)/,/^(?:contains)/,/^(?:tags)/,/^(?:[\w*._:-]+)/,/^(?:"[^"]+")/,/^(?:<=)/,/^(?:<)/,/^(?:>=)/,/^(?:>)/,/^(?:!=)/,/^(?:==)/,/^(?:=)/,/^(?:\|\|)/,/^(?:\|)/,/^(?:&&)/,/^(?:&)/,/^(?:\()/,/^(?:\))/,/^(?:!)/,/^(?:$)/,/^(?:.)/,/^(?:.)/];
lexer.conditions = {"INITIAL":{"rules":[0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42],"inclusive":true}};
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