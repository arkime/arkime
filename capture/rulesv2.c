/******************************************************************************/
/* rulesv2.c  -- Rules v2 expression parser and type checker
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "arkime.h"
#include <ctype.h>
#include <stdarg.h>
#include <errno.h>
#include <arpa/inet.h>
#include "pcap.h"
#include "patricia.h"

/******************************************************************************/
extern ArkimeConfig_t        config;

typedef enum {
    RV2_TOK_EOF,
    RV2_TOK_ERROR,
    RV2_TOK_LPAREN,
    RV2_TOK_RPAREN,
    RV2_TOK_COMMA,
    RV2_TOK_AND,
    RV2_TOK_OR,
    RV2_TOK_NOT,
    RV2_TOK_EQ,
    RV2_TOK_NE,
    RV2_TOK_LT,
    RV2_TOK_LE,
    RV2_TOK_GT,
    RV2_TOK_GE,
    RV2_TOK_PLUS,
    RV2_TOK_MINUS,
    RV2_TOK_STAR,
    RV2_TOK_SLASH,
    RV2_TOK_PERCENT,
    RV2_TOK_WORD,
    RV2_TOK_QUOTED,
    RV2_TOK_REGEX,
    RV2_TOK_LIST,
    RV2_TOK_EXISTS
} RV2TokType;

typedef struct {
    RV2TokType  type;
    int         start;
    int         end;
    const char *msg;
} RV2Tok_t;

typedef enum {
    RV2_NODE_AND,
    RV2_NODE_OR,
    RV2_NODE_NOT,
    RV2_NODE_CMP,           // value op value
    RV2_NODE_MATCH,         // value ==/!= literal(s)
    RV2_NODE_EXISTS,        // field ==/!= EXISTS!
    RV2_NODE_ARITH,
    RV2_NODE_NEG,
    RV2_NODE_CALL,
    RV2_NODE_FIELD,
    RV2_NODE_PLACEHOLDER,   // _ inside any()/all()
    RV2_NODE_INT,
    RV2_NODE_FLOAT,
    RV2_NODE_STR,
    RV2_NODE_BOOL,          // constant condition, only left at the root after folding
    // Parse only, replaced or rejected by the checker
    RV2_NODE_WORD,
    RV2_NODE_QUOTED,
    RV2_NODE_REGEX,
    RV2_NODE_LIST,
    RV2_NODE_EXISTS_TOK
} RV2NodeType;

#define RV2_VT_NONE  0
#define RV2_VT_BOOL  1
#define RV2_VT_INT   2
#define RV2_VT_FLOAT 3
#define RV2_VT_STR   4
#define RV2_VT_IP    5

LOCAL const char *rv2VtNames[] = {"nothing", "condition", "integer", "float", "string", "ip"};

typedef enum {
    RV2_VAL_INT,
    RV2_VAL_INT_RANGE,
    RV2_VAL_FLOAT,
    RV2_VAL_FLOAT_RANGE,
    RV2_VAL_STR,
    RV2_VAL_HEAD,
    RV2_VAL_TAIL,
    RV2_VAL_CONTAINS,
    RV2_VAL_GLOB,
    RV2_VAL_REGEX,
    RV2_VAL_CIDR
} RV2ValueKind;

typedef struct {
    RV2ValueKind     kind;
    union {
        struct {                        // INT, INT_RANGE
            int64_t          imin;
            int64_t          imax;
        };
        struct {                        // FLOAT, FLOAT_RANGE
            double           fmin;
            double           fmax;
        };
        struct {                        // STR, HEAD, TAIL, CONTAINS, GLOB, REGEX
            char            *str;
            int              len;
            GRegex          *regex;
        };
        struct {                        // CIDR, v4 stored in the last 4 bytes
            struct in6_addr  ip;
            uint8_t          family;
            uint8_t          bits;
        };
    };
} RV2Value_t;

#define RV2_VAL_IS_STR(kind) ((kind) >= RV2_VAL_STR && (kind) <= RV2_VAL_REGEX)

typedef struct {
    const char          *name;
    const char          *args;  // F field, s string, c constant string, a ip, n number, i integer, f float, p predicate on _, q quoted constant
    uint8_t              ret;   // RV2_VT_*, NONE means numeric promotion of the args
    uint8_t              flags;
    ArkimeRulesV2FuncCb  cb;    // registered functions only
    gpointer             uw;
} RV2Func_t;

#define RV2_FUNC_SESSION_SETUP_ONLY 0x01
#define RV2_FUNC_PER_VALUE          0x02    // multi valued arg makes a multi valued result
#define RV2_FUNC_PURE               0x04

#define RV2_MAX_ARGS 8

LOCAL GPtrArray *rv2Registered;

LOCAL const RV2Func_t rv2Funcs[] = {
    {"count",       "F",  RV2_VT_INT,  0, NULL, NULL},
    {"has",         "F",  RV2_VT_BOOL, 0, NULL, NULL},
    {"len",         "s",  RV2_VT_INT,  RV2_FUNC_PER_VALUE, NULL, NULL},
    {"lower",       "s",  RV2_VT_STR,  RV2_FUNC_PER_VALUE, NULL, NULL},
    {"upper",       "s",  RV2_VT_STR,  RV2_FUNC_PER_VALUE, NULL, NULL},
    {"startsWith",  "sc", RV2_VT_BOOL, 0, NULL, NULL},
    {"endsWith",    "sc", RV2_VT_BOOL, 0, NULL, NULL},
    {"contains",    "sc", RV2_VT_BOOL, 0, NULL, NULL},
    {"icontains",   "sc", RV2_VT_BOOL, 0, NULL, NULL},
    {"isPrivate",   "a",  RV2_VT_BOOL, 0, NULL, NULL},
    {"isMulticast", "a",  RV2_VT_BOOL, 0, NULL, NULL},
    {"isV4",        "a",  RV2_VT_BOOL, 0, NULL, NULL},
    {"isV6",        "a",  RV2_VT_BOOL, 0, NULL, NULL},
    {"min",         "nn", RV2_VT_NONE, 0, NULL, NULL},
    {"max",         "nn", RV2_VT_NONE, 0, NULL, NULL},
    {"abs",         "n",  RV2_VT_NONE, 0, NULL, NULL},
    {"any",         "Fp", RV2_VT_BOOL, 0, NULL, NULL},
    {"all",         "Fp", RV2_VT_BOOL, 0, NULL, NULL},
    {"bpf",         "q",  RV2_VT_BOOL, RV2_FUNC_SESSION_SETUP_ONLY, NULL, NULL},
    {NULL, NULL, 0, 0, NULL, NULL}
};

/******************************************************************************/
LOCAL const RV2Func_t *rv2_lookup_func(const char *name)
{
    for (const RV2Func_t *func = rv2Funcs; func->name; func++) {
        if (strcmp(func->name, name) == 0)
            return func;
    }
    for (guint i = 0; rv2Registered && i < rv2Registered->len; i++) {
        const RV2Func_t *func = g_ptr_array_index(rv2Registered, i);
        if (strcmp(func->name, name) == 0)
            return func;
    }
    return NULL;
}
/******************************************************************************/
LOCAL int rv2_type_char(char c)
{
    switch (c) {
    case 'i':
        return RV2_VT_INT;
    case 'f':
        return RV2_VT_FLOAT;
    case 's':
        return RV2_VT_STR;
    case 'a':
        return RV2_VT_IP;
    case 'b':
        return RV2_VT_BOOL;
    default:
        return RV2_VT_NONE;
    }
}
/******************************************************************************/
void arkime_rulesv2_register_function(const char *name, const char *signature, uint32_t flags, ArkimeRulesV2FuncCb cb, gpointer uw)
{
    if (!name || !g_ascii_isalpha(name[0]))
        LOGEXIT("ERROR - rules function name '%s' must start with a letter", name);
    for (const char *c = name; *c; c++) {
        if (!g_ascii_isalnum(*c) && *c != '_')
            LOGEXIT("ERROR - rules function name '%s' can only have letters, digits and _", name);
    }
    if (rv2_lookup_func(name))
        LOGEXIT("ERROR - rules function '%s' is already defined", name);
    if (!cb)
        LOGEXIT("ERROR - rules function '%s' needs a callback", name);

    if (!signature || !rv2_type_char(signature[0]) || signature[1] != ':')
        LOGEXIT("ERROR - rules function '%s' signature '%s' must be <return type>:<arg types>", name, signature);
    const char *args = signature + 2;
    if (strlen(args) > RV2_MAX_ARGS)
        LOGEXIT("ERROR - rules function '%s' can have at most %d args", name, RV2_MAX_ARGS);
    for (const char *c = args; *c; c++) {
        if (!rv2_type_char(*c) || *c == 'b')
            LOGEXIT("ERROR - rules function '%s' signature '%s' arg types must be one of i f s a", name, signature);
    }

    RV2Func_t *func = g_new0(RV2Func_t, 1);
    func->name = g_strdup(name);
    func->args = g_strdup(args);
    func->ret = rv2_type_char(signature[0]);
    func->flags = RV2_FUNC_PER_VALUE | ((flags & ARKIME_RULESV2_FUNC_PURE) ? RV2_FUNC_PURE : 0);
    func->cb = cb;
    func->uw = uw;

    if (!rv2Registered)
        rv2Registered = g_ptr_array_new();
    g_ptr_array_add(rv2Registered, func);
}
/******************************************************************************/
struct arkime_rulesv2_node {
    RV2NodeType       type;
    uint8_t           vtype;
    uint8_t           multi;     // value may have zero or many values
    uint8_t           andList;   // MATCH: ]a,b[ every item must match
    uint8_t           textNeg;   // lowered from !=, the source text already reads as negated
    RV2TokType        op;        // CMP/MATCH/EXISTS/ARITH operator
    int               field;
    int               start;     // source span
    int               end;
    char             *str;       // token text, function name, or string literal
    int64_t           i;
    double            f;
    const RV2Func_t  *func;
    GPtrArray        *kids;
    RV2Value_t       *values;
    int               nvalues;
};

typedef struct {
    const char *src;
    int         len;
    int         pos;
    int         ruleType;
    int         placeholderType;
    int         placeholderDepth;
    int         nesting;
    int         errorPos;
    char       *error;
} RV2Parser_t;

// Parens, ! and - nesting, every later pass recurses this deep
#define RV2_MAX_NEST  256

// any()/all() nesting, placeholder for depth d lives in register d
#define RV2_MAX_DEPTH 8
#define RV2_MAX_REGS  64

#define RV2_PREC_OR   1
#define RV2_PREC_AND  2
#define RV2_PREC_CMP  3
#define RV2_PREC_ADD  4
#define RV2_PREC_MUL  5

/******************************************************************************/
LOCAL void rv2_error(RV2Parser_t *p, int pos, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
LOCAL void rv2_error(RV2Parser_t *p, int pos, const char *fmt, ...)
{
    if (p->error)
        return;

    va_list args;
    va_start(args, fmt);
    p->error = g_strdup_vprintf(fmt, args);
    va_end(args);
    p->errorPos = pos;
}
/******************************************************************************/
LOCAL ArkimeRulesV2Node_t *rv2_node_new(RV2NodeType type, int start, int end)
{
    ArkimeRulesV2Node_t *node = g_new0(ArkimeRulesV2Node_t, 1);
    node->type = type;
    node->start = start;
    node->end = end;
    node->field = -1;
    return node;
}
/******************************************************************************/
LOCAL void rv2_node_add_kid(ArkimeRulesV2Node_t *node, ArkimeRulesV2Node_t *kid)
{
    if (!node->kids)
        node->kids = g_ptr_array_new_with_free_func((GDestroyNotify)arkime_rulesv2_free);
    g_ptr_array_add(node->kids, kid);
    if (kid->end > node->end)
        node->end = kid->end;
}
#define RV2_KID(node, i) ((ArkimeRulesV2Node_t *)g_ptr_array_index((node)->kids, i))
#define RV2_NKIDS(node) ((node)->kids ? (int)(node)->kids->len : 0)
/******************************************************************************/
LOCAL void rv2_value_free(RV2Value_t *value)
{
    if (!RV2_VAL_IS_STR(value->kind))
        return;
    g_free(value->str);
    if (value->regex)
        g_regex_unref(value->regex);
}
/******************************************************************************/
void arkime_rulesv2_free(ArkimeRulesV2Node_t *node)
{
    if (!node)
        return;

    if (node->kids)
        g_ptr_array_free(node->kids, TRUE);

    for (int v = 0; v < node->nvalues; v++)
        rv2_value_free(&node->values[v]);
    g_free(node->values);
    g_free(node->str);
    g_free(node);
}
/******************************************************************************/
/* Lexer, same token shapes as viewer/arkimeparser.jison. A '/' starts a regex
 * only where a value is expected, otherwise it is division. Operators are only
 * recognized between values, so field names and values keep their - + * / chars
 * and arithmetic needs whitespace around the operator. */
LOCAL int rv2_is_word_char(char c)
{
    return c && (isalnum((unsigned char)c) || strchr("-+_.@:*?/$", c) != NULL);
}
/******************************************************************************/
LOCAL int rv2_scan_word(const RV2Parser_t *p, int i)
{
    while (i < p->len && rv2_is_word_char(p->src[i]))
        i++;
    return i;
}
/******************************************************************************/
// Returns the index after the closing char, or -1 if there isn't one
LOCAL int rv2_scan_delim(const RV2Parser_t *p, int i, char close)
{
    for (i++; i < p->len; i++) {
        if (p->src[i] == '\\') {
            i++;
            continue;
        }
        if (p->src[i] == close)
            return i + 1;
    }
    return -1;
}
/******************************************************************************/
LOCAL RV2Tok_t rv2_peek(const RV2Parser_t *p, gboolean valueMode)
{
    RV2Tok_t tok;
    int i = p->pos;

    while (i < p->len && isspace((unsigned char)p->src[i]))
        i++;

    tok.start = i;
    tok.end = i + 1;
    tok.msg = NULL;

    if (i >= p->len) {
        tok.type = RV2_TOK_EOF;
        tok.end = i;
        return tok;
    }

    const char c = p->src[i];
    const char n = (i + 1 < p->len) ? p->src[i + 1] : 0;

    switch (c) {
    case '(':
        tok.type = RV2_TOK_LPAREN;
        return tok;
    case ')':
        tok.type = RV2_TOK_RPAREN;
        return tok;
    case ',':
        tok.type = RV2_TOK_COMMA;
        return tok;
    case '&':
        tok.type = RV2_TOK_AND;
        if (n == '&')
            tok.end++;
        return tok;
    case '|':
        tok.type = RV2_TOK_OR;
        if (n == '|')
            tok.end++;
        return tok;
    case '=':
        tok.type = RV2_TOK_EQ;
        if (n == '=')
            tok.end++;
        return tok;
    case '<':
        tok.type = RV2_TOK_LT;
        if (n == '=') {
            tok.type = RV2_TOK_LE;
            tok.end++;
        }
        return tok;
    case '>':
        tok.type = RV2_TOK_GT;
        if (n == '=') {
            tok.type = RV2_TOK_GE;
            tok.end++;
        }
        return tok;
    case '!':
        tok.type = RV2_TOK_NOT;
        if (n == '=') {
            tok.type = RV2_TOK_NE;
            tok.end++;
        }
        return tok;
    }

    if (!valueMode) {
        switch (c) {
        case '+':
            tok.type = RV2_TOK_PLUS;
            return tok;
        case '-':
            tok.type = RV2_TOK_MINUS;
            return tok;
        case '*':
            tok.type = RV2_TOK_STAR;
            return tok;
        case '/':
            tok.type = RV2_TOK_SLASH;
            return tok;
        case '%':
            tok.type = RV2_TOK_PERCENT;
            return tok;
        }
        tok.type = RV2_TOK_ERROR;
        tok.msg = "expected an operator such as &&, ||, ==";
        return tok;
    }

    switch (c) {
    case '"':
        tok.type = RV2_TOK_QUOTED;
        tok.end = rv2_scan_delim(p, i, '"');
        if (tok.end == -1) {
            tok.type = RV2_TOK_ERROR;
            tok.msg = "unterminated quoted string";
        }
        return tok;
    case '[':
    case ']':
        tok.type = RV2_TOK_LIST;
        tok.end = rv2_scan_delim(p, i, c == '[' ? ']' : '[');
        if (tok.end == -1) {
            tok.type = RV2_TOK_ERROR;
            tok.msg = "unterminated list";
        }
        return tok;
    case '/': {
        // Longest match wins, a tie is a regex, same as the viewer's lexer
        const int rend = rv2_scan_delim(p, i, '/');
        const int wend = rv2_scan_word(p, i);
        if (rend != -1 && rend >= wend) {
            tok.type = RV2_TOK_REGEX;
            tok.end = rend;
        } else {
            tok.type = RV2_TOK_WORD;
            tok.end = wend;
        }
        return tok;
    }
    case '-':
        if (!rv2_is_word_char(n)) {
            tok.type = RV2_TOK_MINUS;
            return tok;
        }
        break;
    }

    if (p->len - i >= 7 && strncmp(p->src + i, "EXISTS!", 7) == 0) {
        tok.type = RV2_TOK_EXISTS;
        tok.end = i + 7;
        return tok;
    }

    if (rv2_is_word_char(c)) {
        tok.type = RV2_TOK_WORD;
        tok.end = rv2_scan_word(p, i);
        return tok;
    }

    tok.type = RV2_TOK_ERROR;
    tok.msg = "unexpected character";
    return tok;
}
/******************************************************************************/
LOCAL const char *rv2_op_str(RV2TokType op)
{
    switch (op) {
    case RV2_TOK_EQ:
        return "==";
    case RV2_TOK_NE:
        return "!=";
    case RV2_TOK_LT:
        return "<";
    case RV2_TOK_LE:
        return "<=";
    case RV2_TOK_GT:
        return ">";
    case RV2_TOK_GE:
        return ">=";
    case RV2_TOK_PLUS:
        return "+";
    case RV2_TOK_MINUS:
        return "-";
    case RV2_TOK_STAR:
        return "*";
    case RV2_TOK_SLASH:
        return "/";
    case RV2_TOK_PERCENT:
        return "%";
    default:
        return "?";
    }
}
/******************************************************************************/
LOCAL int rv2_infix_prec(RV2TokType type)
{
    switch (type) {
    case RV2_TOK_OR:
        return RV2_PREC_OR;
    case RV2_TOK_AND:
        return RV2_PREC_AND;
    case RV2_TOK_EQ:
    case RV2_TOK_NE:
    case RV2_TOK_LT:
    case RV2_TOK_LE:
    case RV2_TOK_GT:
    case RV2_TOK_GE:
        return RV2_PREC_CMP;
    case RV2_TOK_PLUS:
    case RV2_TOK_MINUS:
        return RV2_PREC_ADD;
    case RV2_TOK_STAR:
    case RV2_TOK_SLASH:
    case RV2_TOK_PERCENT:
        return RV2_PREC_MUL;
    default:
        return 0;
    }
}
/******************************************************************************/
LOCAL void rv2_error_tok(RV2Parser_t *p, const RV2Tok_t *tok, const char *what)
{
    if (tok->type == RV2_TOK_ERROR)
        rv2_error(p, tok->start, "%s", tok->msg);
    else if (tok->type == RV2_TOK_EOF)
        rv2_error(p, tok->start, "unexpected end of expression, %s", what);
    else
        rv2_error(p, tok->start, "unexpected '%.*s', %s", tok->end - tok->start, p->src + tok->start, what);
}
/******************************************************************************/
LOCAL ArkimeRulesV2Node_t *rv2_parse_expr(RV2Parser_t *p, int minPrec);

LOCAL ArkimeRulesV2Node_t *rv2_parse_prefix_inner(RV2Parser_t *p);

LOCAL ArkimeRulesV2Node_t *rv2_parse_prefix(RV2Parser_t *p)
{
    if (p->nesting >= RV2_MAX_NEST) {
        RV2Tok_t tok = rv2_peek(p, TRUE);
        rv2_error(p, tok.start, "expression is nested more than %d deep", RV2_MAX_NEST);
        return NULL;
    }
    p->nesting++;
    ArkimeRulesV2Node_t *node = rv2_parse_prefix_inner(p);
    p->nesting--;
    return node;
}
/******************************************************************************/
LOCAL ArkimeRulesV2Node_t *rv2_parse_prefix_inner(RV2Parser_t *p)
{
    RV2Tok_t tok = rv2_peek(p, TRUE);
    ArkimeRulesV2Node_t *node, *kid;

    switch (tok.type) {
    case RV2_TOK_NOT:
        p->pos = tok.end;
        kid = rv2_parse_expr(p, RV2_PREC_CMP);
        if (!kid)
            return NULL;
        node = rv2_node_new(RV2_NODE_NOT, tok.start, tok.end);
        rv2_node_add_kid(node, kid);
        return node;

    case RV2_TOK_MINUS:
        p->pos = tok.end;
        kid = rv2_parse_prefix(p);
        if (!kid)
            return NULL;
        node = rv2_node_new(RV2_NODE_NEG, tok.start, tok.end);
        rv2_node_add_kid(node, kid);
        return node;

    case RV2_TOK_LPAREN: {
        p->pos = tok.end;
        node = rv2_parse_expr(p, RV2_PREC_OR);
        if (!node)
            return NULL;
        RV2Tok_t close = rv2_peek(p, FALSE);
        if (close.type != RV2_TOK_RPAREN) {
            rv2_error_tok(p, &close, "expected ')'");
            arkime_rulesv2_free(node);
            return NULL;
        }
        p->pos = close.end;
        return node;
    }

    case RV2_TOK_WORD: {
        p->pos = tok.end;
        RV2Tok_t open = rv2_peek(p, FALSE);
        if (open.type != RV2_TOK_LPAREN) {
            node = rv2_node_new(RV2_NODE_WORD, tok.start, tok.end);
            node->str = g_strndup(p->src + tok.start, tok.end - tok.start);
            return node;
        }

        p->pos = open.end;
        node = rv2_node_new(RV2_NODE_CALL, tok.start, open.end);
        node->str = g_strndup(p->src + tok.start, tok.end - tok.start);

        RV2Tok_t next = rv2_peek(p, TRUE);
        if (next.type == RV2_TOK_RPAREN) {
            p->pos = next.end;
            node->end = next.end;
            return node;
        }

        while (1) {
            kid = rv2_parse_expr(p, RV2_PREC_OR);
            if (!kid) {
                arkime_rulesv2_free(node);
                return NULL;
            }
            rv2_node_add_kid(node, kid);

            next = rv2_peek(p, FALSE);
            p->pos = next.end;
            if (next.type == RV2_TOK_RPAREN) {
                node->end = next.end;
                return node;
            }
            if (next.type != RV2_TOK_COMMA) {
                rv2_error_tok(p, &next, "expected ',' or ')'");
                arkime_rulesv2_free(node);
                return NULL;
            }
        }
    }

    case RV2_TOK_QUOTED:
    case RV2_TOK_REGEX:
    case RV2_TOK_LIST:
    case RV2_TOK_EXISTS:
        p->pos = tok.end;
        node = rv2_node_new(tok.type == RV2_TOK_QUOTED ? RV2_NODE_QUOTED :
                            tok.type == RV2_TOK_REGEX  ? RV2_NODE_REGEX :
                            tok.type == RV2_TOK_LIST   ? RV2_NODE_LIST : RV2_NODE_EXISTS_TOK,
                            tok.start, tok.end);
        node->str = g_strndup(p->src + tok.start, tok.end - tok.start);
        return node;

    default:
        rv2_error_tok(p, &tok, "expected a field, value, '(' or '!'");
        return NULL;
    }
}
/******************************************************************************/
LOCAL ArkimeRulesV2Node_t *rv2_parse_expr(RV2Parser_t *p, int minPrec)
{
    ArkimeRulesV2Node_t *left = rv2_parse_prefix(p);
    if (!left)
        return NULL;

    while (1) {
        RV2Tok_t tok = rv2_peek(p, FALSE);
        const int prec = rv2_infix_prec(tok.type);

        if (prec == 0 || prec < minPrec) {
            // Let the caller decide if what follows is ok
            if (tok.type == RV2_TOK_ERROR && minPrec <= RV2_PREC_OR) {
                rv2_error_tok(p, &tok, "");
                arkime_rulesv2_free(left);
                return NULL;
            }
            return left;
        }

        p->pos = tok.end;
        ArkimeRulesV2Node_t *right = rv2_parse_expr(p, prec + 1);
        if (!right) {
            arkime_rulesv2_free(left);
            return NULL;
        }

        if (prec == RV2_PREC_AND || prec == RV2_PREC_OR) {
            const RV2NodeType type = (prec == RV2_PREC_AND) ? RV2_NODE_AND : RV2_NODE_OR;
            if (left->type != type) {
                ArkimeRulesV2Node_t *node = rv2_node_new(type, left->start, left->end);
                rv2_node_add_kid(node, left);
                left = node;
            }
            rv2_node_add_kid(left, right);
            continue;
        }

        ArkimeRulesV2Node_t *node = rv2_node_new(prec == RV2_PREC_CMP ? RV2_NODE_CMP : RV2_NODE_ARITH, left->start, left->end);
        node->op = tok.type;
        rv2_node_add_kid(node, left);
        rv2_node_add_kid(node, right);
        left = node;

        if (prec == RV2_PREC_CMP) {
            RV2Tok_t next = rv2_peek(p, FALSE);
            if (rv2_infix_prec(next.type) == RV2_PREC_CMP) {
                rv2_error(p, next.start, "comparisons can't be chained, use && between them");
                arkime_rulesv2_free(left);
                return NULL;
            }
        }
    }
}
/******************************************************************************/
/* Type checker */
/******************************************************************************/
LOCAL int rv2_lookup_field(const char *name)
{
    if (strncmp(name, "db:", 3) == 0)
        return arkime_field_by_db(name + 3);
    return arkime_field_by_exp_ignore_error(name);
}
/******************************************************************************/
LOCAL int rv2_field_vtype(int pos, uint8_t *multi)
{
    const ArkimeFieldInfo_t *info = config.fields[pos];

    if (info->cntForPos) {
        *multi = 0;
        return RV2_VT_INT;
    }

    const ArkimeFieldType t = info->type;
    *multi = !(t == ARKIME_FIELD_TYPE_INT || t == ARKIME_FIELD_TYPE_STR || t == ARKIME_FIELD_TYPE_IP || t == ARKIME_FIELD_TYPE_FLOAT);

    if (ARKIME_FIELD_TYPE_IS_INT(t))
        return RV2_VT_INT;
    if (ARKIME_FIELD_TYPE_IS_STR(t))
        return RV2_VT_STR;
    if (ARKIME_FIELD_TYPE_IS_IP(t))
        return RV2_VT_IP;
    if (ARKIME_FIELD_TYPE_IS_FLOAT(t))
        return RV2_VT_FLOAT;
    return RV2_VT_NONE;
}
/******************************************************************************/
LOCAL gboolean rv2_parse_int(const char *str, int64_t *out)
{
    if (!*str || (!isdigit((unsigned char)str[0]) && !(str[0] == '-' && isdigit((unsigned char)str[1]))))
        return FALSE;

    char *end;
    errno = 0;
    long long v = strtoll(str, &end, 10);
    if (errno || *end)
        return FALSE;
    *out = v;
    return TRUE;
}
/******************************************************************************/
LOCAL gboolean rv2_parse_float(const char *str, double *out)
{
    const char *digits = str[0] == '-' ? str + 1 : str;
    if (!isdigit((unsigned char)digits[0]) && !(digits[0] == '.' && isdigit((unsigned char)digits[1])))
        return FALSE;
    if (strpbrk(str, "xXnN"))
        return FALSE;

    char *end;
    errno = 0;
    double v = g_ascii_strtod(str, &end);
    if (errno || *end || end == str)
        return FALSE;
    *out = v;
    return TRUE;
}
/******************************************************************************/
// Split "a-b" ranges, the first char may be a minus sign
LOCAL const char *rv2_range_dash(const char *str)
{
    if (!*str)
        return NULL;
    return strchr(str + 1, '-');
}
/******************************************************************************/
LOCAL gboolean rv2_is_number(const char *str)
{
    double f;
    return rv2_parse_float(str, &f);
}
/******************************************************************************/
LOCAL int rv2_check(RV2Parser_t *p, ArkimeRulesV2Node_t *node);

/* Turn a bare word into a field, number, or _.  literalFirst prefers numbers
 * over fields, used on the right side of comparisons. */
LOCAL int rv2_resolve_word(RV2Parser_t *p, ArkimeRulesV2Node_t *node, gboolean literalFirst)
{
    const char *str = node->str;

    if (strcmp(str, "_") == 0) {
        if (!p->placeholderType) {
            rv2_error(p, node->start, "'_' can only be used inside any() or all()");
            return 0;
        }
        node->type = RV2_NODE_PLACEHOLDER;
        node->vtype = p->placeholderType;
        node->i = p->placeholderDepth - 1;
        return node->vtype;
    }

    if (str[0] == '$') {
        rv2_error(p, node->start, "shortcuts like '%s' aren't supported in capture rules", str);
        return 0;
    }

    int pos = -1;
    if (!literalFirst || !rv2_is_number(str))
        pos = rv2_lookup_field(str);

    // The lexer keeps a leading - on words, so -field is negation
    if (pos < 0 && str[0] == '-' && rv2_lookup_field(str + 1) >= 0) {
        ArkimeRulesV2Node_t *kid = rv2_node_new(RV2_NODE_WORD, node->start + 1, node->end);
        kid->str = g_strdup(str + 1);
        g_free(node->str);
        node->str = NULL;
        node->type = RV2_NODE_NEG;
        rv2_node_add_kid(node, kid);
        return rv2_check(p, node);
    }

    if (pos >= 0) {
        node->type = RV2_NODE_FIELD;
        node->field = pos;
        node->vtype = rv2_field_vtype(pos, &node->multi);
        if (node->vtype == RV2_VT_NONE) {
            rv2_error(p, node->start, "field '%s' is an object field, which rules don't support", str);
            return 0;
        }
        return node->vtype;
    }

    if (rv2_parse_int(str, &node->i)) {
        node->type = RV2_NODE_INT;
        node->vtype = RV2_VT_INT;
        return node->vtype;
    }

    if (rv2_parse_float(str, &node->f)) {
        node->type = RV2_NODE_FLOAT;
        node->vtype = RV2_VT_FLOAT;
        return node->vtype;
    }

    rv2_error(p, node->start, "unknown field '%s'", str);
    return 0;
}
/******************************************************************************/
// Remove surrounding quotes and backslash escapes
LOCAL char *rv2_unquote(const char *str, int len)
{
    if (len >= 2 && str[0] == '"' && str[len - 1] == '"') {
        str++;
        len -= 2;
    }

    char *out = g_malloc(len + 1);
    int o = 0;
    for (int i = 0; i < len; i++) {
        if (str[i] == '\\' && i + 1 < len)
            i++;
        out[o++] = str[i];
    }
    out[o] = 0;
    return out;
}
/******************************************************************************/
LOCAL gboolean rv2_parse_value_str(RV2Parser_t *p, RV2Value_t *value, const char *item, int itemLen, int itemPos, char strKind)
{
    if (itemLen >= 2 && item[0] == '/' && item[itemLen - 1] == '/') {
        // Full string match like the viewer's regexp queries
        value->kind = RV2_VAL_REGEX;
        value->str = g_strndup(item + 1, itemLen - 2);
        char *pattern = g_strdup_printf("\\A(?:%.*s)\\z", itemLen - 2, item + 1);
        GError *error = NULL;
        value->regex = g_regex_new(pattern, G_REGEX_OPTIMIZE, 0, &error);
        g_free(pattern);
        if (!value->regex) {
            rv2_error(p, itemPos, "bad regex %.*s: %s", itemLen, item, error->message);
            g_error_free(error);
            return FALSE;
        }
        return TRUE;
    }

    char *str = rv2_unquote(item, itemLen);
    if (strKind == ARKIME_FIELD_STRKIND_LOWER) {
        char *tmp = g_utf8_strdown(str, -1);
        g_free(str);
        str = tmp;
    } else if (strKind == ARKIME_FIELD_STRKIND_UPPER) {
        char *tmp = g_utf8_strup(str, -1);
        g_free(str);
        str = tmp;
    }

    int len = strlen(str);
    value->kind = RV2_VAL_STR;

    if (strchr(str, '*')) {
        if (strcmp(str, "*") == 0) {
            g_free(str);
            rv2_error(p, itemPos, "use 'EXISTS!' instead of '*'");
            return FALSE;
        }

        const char *inner = str;
        int innerLen = len;
        gboolean lead = str[0] == '*';
        gboolean trail = len > 1 && str[len - 1] == '*';
        if (lead) {
            inner++;
            innerLen--;
        }
        if (trail)
            innerLen--;

        if (innerLen > 0 && !memchr(inner, '*', innerLen) && !memchr(inner, '?', innerLen)) {
            value->kind = lead && trail ? RV2_VAL_CONTAINS : (lead ? RV2_VAL_TAIL : RV2_VAL_HEAD);
            char *tmp = g_strndup(inner, innerLen);
            g_free(str);
            str = tmp;
            len = innerLen;
        } else {
            value->kind = RV2_VAL_GLOB;
        }
    }

    value->str = str;
    value->len = len;
    return TRUE;
}
/******************************************************************************/
LOCAL gboolean rv2_parse_value_ip(RV2Parser_t *p, RV2Value_t *value, const char *str, int itemPos)
{
    value->kind = RV2_VAL_CIDR;

    if (strcmp(str, "ipv4") == 0) {
        value->family = 4;
        return TRUE;
    }
    if (strcmp(str, "ipv6") == 0) {
        value->family = 6;
        return TRUE;
    }

    char addr[INET6_ADDRSTRLEN + 1];
    const char *slash = strchr(str, '/');
    int addrLen = slash ? slash - str : (int)strlen(str);
    if (addrLen == 0 || addrLen > INET6_ADDRSTRLEN)
        goto bad;
    memcpy(addr, str, addrLen);
    addr[addrLen] = 0;

    int maxBits;
    if (strchr(addr, ':')) {
        if (inet_pton(AF_INET6, addr, &value->ip) != 1)
            goto bad;
        value->family = 6;
        maxBits = 128;
    } else {
        memset(&value->ip, 0, sizeof(value->ip));
        if (inet_pton(AF_INET, addr, ((uint8_t *)&value->ip) + 12) != 1)
            goto bad;
        value->family = 4;
        maxBits = 32;
    }

    int64_t bits = maxBits;
    if (slash && (!rv2_parse_int(slash + 1, &bits) || bits < 0 || bits > maxBits))
        goto bad;
    value->bits = bits;
    return TRUE;

bad:
    rv2_error(p, itemPos, "'%s' is not an ip or cidr", str);
    return FALSE;
}
/******************************************************************************/
LOCAL gboolean rv2_parse_value(RV2Parser_t *p, RV2Value_t *value, int vtype, const char *item, int itemLen, int itemPos, char strKind)
{
    if (vtype == RV2_VT_STR)
        return rv2_parse_value_str(p, value, item, itemLen, itemPos, strKind);

    if (itemLen >= 2 && item[0] == '/' && item[itemLen - 1] == '/') {
        rv2_error(p, itemPos, "regex can only be used with string values, not %s", rv2VtNames[vtype]);
        return FALSE;
    }

    char *str = rv2_unquote(item, itemLen);
    gboolean ok = FALSE;
    const char *dash;

    switch (vtype) {
    case RV2_VT_INT:
        if (rv2_parse_int(str, &value->imin)) {
            value->kind = RV2_VAL_INT;
            value->imax = value->imin;
            ok = TRUE;
        } else if ((dash = rv2_range_dash(str))) {
            char *min = g_strndup(str, dash - str);
            ok = rv2_parse_int(min, &value->imin) && rv2_parse_int(dash + 1, &value->imax);
            g_free(min);
            if (ok && value->imin > value->imax) {
                rv2_error(p, itemPos, "range '%s' has min > max", str);
                g_free(str);
                return FALSE;
            }
            value->kind = RV2_VAL_INT_RANGE;
        }
        if (!ok)
            rv2_error(p, itemPos, "'%s' is not an integer or integer range", str);
        break;

    case RV2_VT_FLOAT:
        if (rv2_parse_float(str, &value->fmin)) {
            value->kind = RV2_VAL_FLOAT;
            value->fmax = value->fmin;
            ok = TRUE;
        } else if ((dash = rv2_range_dash(str))) {
            char *min = g_strndup(str, dash - str);
            ok = rv2_parse_float(min, &value->fmin) && rv2_parse_float(dash + 1, &value->fmax);
            g_free(min);
            if (ok && value->fmin > value->fmax) {
                rv2_error(p, itemPos, "range '%s' has min > max", str);
                g_free(str);
                return FALSE;
            }
            value->kind = RV2_VAL_FLOAT_RANGE;
        }
        if (!ok)
            rv2_error(p, itemPos, "'%s' is not a number or number range", str);
        break;

    case RV2_VT_IP:
        ok = rv2_parse_value_ip(p, value, str, itemPos);
        break;

    default:
        rv2_error(p, itemPos, "can't compare a %s to '%s'", rv2VtNames[vtype], str);
        break;
    }

    g_free(str);
    return ok;
}
/******************************************************************************/
/* Build the value list for a MATCH node from a single value token or a
 * [a,b] (any) or ]a,b[ (all) list. List items are split on unescaped commas. */
LOCAL gboolean rv2_match_values(RV2Parser_t *p, ArkimeRulesV2Node_t *node, const ArkimeRulesV2Node_t *lhs, const ArkimeRulesV2Node_t *rhs)
{
    const char strKind = (lhs->type == RV2_NODE_FIELD) ? config.fields[lhs->field]->strKind : 0;
    const char *text = p->src + rhs->start;
    const int textLen = rhs->end - rhs->start;

    if (rhs->type != RV2_NODE_LIST) {
        if (text[0] == '$') {
            rv2_error(p, rhs->start, "shortcuts like '%.*s' aren't supported in capture rules", textLen, text);
            return FALSE;
        }
        node->values = g_new0(RV2Value_t, 1);
        node->nvalues = 1;
        return rv2_parse_value(p, &node->values[0], lhs->vtype, text, textLen, rhs->start, strKind);
    }

    node->andList = text[0] == ']';
    node->values = g_new0(RV2Value_t, textLen / 2 + 1);

    int i = 1;
    const int last = textLen - 1;
    while (i <= last) {
        while (i < last && isspace((unsigned char)text[i]))
            i++;
        int start = i;
        gboolean inQuote = FALSE;
        while (i < last && (inQuote || text[i] != ',')) {
            if (text[i] == '\\')
                i++;
            else if (text[i] == '"')
                inQuote = !inQuote;
            i++;
        }
        if (i > last)
            i = last;
        int end = i;
        while (end > start && isspace((unsigned char)text[end - 1]))
            end--;

        if (end == start) {
            if (i == last && node->nvalues > 0)
                break;
            rv2_error(p, rhs->start + start, "empty list item");
            return FALSE;
        }
        if (text[start] == '$') {
            rv2_error(p, rhs->start + start, "shortcuts like '%.*s' aren't supported in capture rules", end - start, text + start);
            return FALSE;
        }

        if (!rv2_parse_value(p, &node->values[node->nvalues], lhs->vtype, text + start, end - start, rhs->start + start, strKind))
            return FALSE;
        node->nvalues++;
        i++;
    }

    if (node->nvalues == 0) {
        rv2_error(p, rhs->start, "empty list");
        return FALSE;
    }
    return TRUE;
}
/******************************************************************************/
LOCAL gboolean rv2_check_bool(RV2Parser_t *p, ArkimeRulesV2Node_t *node)
{
    int vt = rv2_check(p, node);
    if (!vt)
        return FALSE;
    if (vt == RV2_VT_BOOL)
        return TRUE;

    if (node->type == RV2_NODE_FIELD)
        rv2_error(p, node->start, "'%s' is a field, not a condition, did you mean '%s == EXISTS!'?", node->str, node->str);
    else
        rv2_error(p, node->start, "expected a condition but found a %s value", rv2VtNames[vt]);
    return FALSE;
}
/******************************************************************************/
LOCAL gboolean rv2_is_numeric(int vt)
{
    return vt == RV2_VT_INT || vt == RV2_VT_FLOAT;
}
/******************************************************************************/
LOCAL int rv2_check_value(RV2Parser_t *p, ArkimeRulesV2Node_t *node, gboolean literalFirst)
{
    int vt;
    if (node->type == RV2_NODE_WORD)
        vt = rv2_resolve_word(p, node, literalFirst);
    else
        vt = rv2_check(p, node);

    if (vt == RV2_VT_BOOL) {
        rv2_error(p, node->start, "expected a value but found a condition");
        return 0;
    }
    return vt;
}
/******************************************************************************/
LOCAL int rv2_check_cmp(RV2Parser_t *p, ArkimeRulesV2Node_t *node)
{
    ArkimeRulesV2Node_t *lhs = RV2_KID(node, 0);
    ArkimeRulesV2Node_t *rhs = RV2_KID(node, 1);
    const char *opStr = rv2_op_str(node->op);

    const int lt = rv2_check_value(p, lhs, FALSE);
    if (!lt)
        return 0;

    node->vtype = RV2_VT_BOOL;

    if (node->op == RV2_TOK_EQ || node->op == RV2_TOK_NE) {
        if (rhs->type == RV2_NODE_EXISTS_TOK) {
            if (lhs->type != RV2_NODE_FIELD) {
                rv2_error(p, lhs->start, "EXISTS! can only be used with a field");
                return 0;
            }
            node->type = RV2_NODE_EXISTS;
            node->field = lhs->field;
            g_ptr_array_remove_index(node->kids, 1);
            return node->vtype;
        }

        gboolean literal = rhs->type == RV2_NODE_LIST || rhs->type == RV2_NODE_REGEX || rhs->type == RV2_NODE_QUOTED;

        // A bare word on the right is a value, same as the viewer. For non string
        // fields fall back to a field name if it isn't a valid value.
        if (rhs->type == RV2_NODE_WORD) {
            literal = TRUE;
            int64_t i;
            if (lt == RV2_VT_INT && !rv2_parse_int(rhs->str, &i) && rv2_is_number(rhs->str))
                literal = FALSE;  // integer == 7.5 is a numeric compare
            if (lt != RV2_VT_STR && rhs->str[0] != '$' && rv2_lookup_field(rhs->str) >= 0) {
                RV2Parser_t tmp = *p;
                RV2Value_t value;
                memset(&value, 0, sizeof(value));
                tmp.error = NULL;
                literal = rv2_parse_value(&tmp, &value, lt, rhs->str, strlen(rhs->str), rhs->start, 0);
                g_free(tmp.error);
                rv2_value_free(&value);
            }
        }

        if (literal) {
            node->type = RV2_NODE_MATCH;
            if (!rv2_match_values(p, node, lhs, rhs))
                return 0;
            g_ptr_array_remove_index(node->kids, 1);
            return node->vtype;
        }

        const int rt = rv2_check_value(p, rhs, FALSE);
        if (!rt)
            return 0;
        if (lt != rt && !(rv2_is_numeric(lt) && rv2_is_numeric(rt))) {
            rv2_error(p, rhs->start, "can't compare a %s to a %s", rv2VtNames[lt], rv2VtNames[rt]);
            return 0;
        }
        return node->vtype;
    }

    if (!rv2_is_numeric(lt)) {
        rv2_error(p, lhs->start, "'%s' needs a number on the left, found a %s", opStr, rv2VtNames[lt]);
        return 0;
    }

    if (rhs->type == RV2_NODE_LIST || rhs->type == RV2_NODE_REGEX || rhs->type == RV2_NODE_QUOTED || rhs->type == RV2_NODE_EXISTS_TOK) {
        rv2_error(p, rhs->start, "'%s' needs a number on the right", opStr);
        return 0;
    }

    const int rt = rv2_check_value(p, rhs, TRUE);
    if (!rt)
        return 0;
    if (!rv2_is_numeric(rt)) {
        rv2_error(p, rhs->start, "'%s' needs a number on the right, found a %s", opStr, rv2VtNames[rt]);
        return 0;
    }
    return node->vtype;
}
/******************************************************************************/
LOCAL int rv2_check_arith(RV2Parser_t *p, ArkimeRulesV2Node_t *node)
{
    int vt[2];
    for (int k = 0; k < 2; k++) {
        ArkimeRulesV2Node_t *kid = RV2_KID(node, k);
        vt[k] = rv2_check_value(p, kid, TRUE);
        if (!vt[k])
            return 0;
        if (!rv2_is_numeric(vt[k])) {
            rv2_error(p, kid->start, "'%s' needs numbers, found a %s", rv2_op_str(node->op), rv2VtNames[vt[k]]);
            return 0;
        }
        if (kid->multi) {
            rv2_error(p, kid->start, "multi value field can't be used in math, use count() for the number of values");
            return 0;
        }
    }

    if (node->op == RV2_TOK_PERCENT && (vt[0] != RV2_VT_INT || vt[1] != RV2_VT_INT)) {
        rv2_error(p, node->start, "'%%' needs integers");
        return 0;
    }

    node->vtype = (vt[0] == RV2_VT_INT && vt[1] == RV2_VT_INT) ? RV2_VT_INT : RV2_VT_FLOAT;
    return node->vtype;
}
/******************************************************************************/
LOCAL int rv2_check_call(RV2Parser_t *p, ArkimeRulesV2Node_t *node)
{
    const RV2Func_t *func = rv2_lookup_func(node->str);
    if (!func) {
        rv2_error(p, node->start, "unknown function '%s'", node->str);
        return 0;
    }
    node->func = func;

    const int nargs = strlen(func->args);
    if (RV2_NKIDS(node) != nargs) {
        rv2_error(p, node->start, "%s() takes %d argument%s, found %d", func->name, nargs, nargs == 1 ? "" : "s", RV2_NKIDS(node));
        return 0;
    }

    if ((func->flags & RV2_FUNC_SESSION_SETUP_ONLY) && p->ruleType != ARKIME_RULE_TYPE_SESSION_SETUP) {
        rv2_error(p, node->start, "%s() can only be used in sessionSetup rules", func->name);
        return 0;
    }

    int numVt = RV2_VT_INT;
    for (int a = 0; a < nargs; a++) {
        ArkimeRulesV2Node_t *arg = RV2_KID(node, a);
        int vt;

        switch (func->args[a]) {
        case 'F':
            if (arg->type != RV2_NODE_WORD || !rv2_resolve_word(p, arg, FALSE))
                goto needField;
            if (arg->type != RV2_NODE_FIELD)
                goto needField;
            break;

        case 'p': {
            if (p->placeholderDepth + 1 >= RV2_MAX_DEPTH) {
                rv2_error(p, node->start, "any() and all() can only be nested %d deep", RV2_MAX_DEPTH - 2);
                return 0;
            }
            const int saved = p->placeholderType;
            p->placeholderType = RV2_KID(node, 0)->vtype;
            p->placeholderDepth++;
            gboolean ok = rv2_check_bool(p, arg);
            p->placeholderDepth--;
            p->placeholderType = saved;
            if (!ok)
                return 0;
            break;
        }

        case 'c':
        case 'q':
            if (arg->type == RV2_NODE_QUOTED || (func->args[a] == 'c' && arg->type == RV2_NODE_WORD)) {
                char *str = rv2_unquote(arg->str, strlen(arg->str));
                g_free(arg->str);
                arg->str = str;
                arg->type = RV2_NODE_STR;
                arg->vtype = RV2_VT_STR;
                break;
            }
            rv2_error(p, arg->start, "%s() argument %d must be a %sstring constant", func->name, a + 1, func->args[a] == 'q' ? "quoted " : "");
            return 0;

        case 's':
        case 'a':
        case 'n':
        case 'i':
        case 'f':
            if (arg->type == RV2_NODE_QUOTED) {
                char *str = rv2_unquote(arg->str, strlen(arg->str));
                g_free(arg->str);
                arg->str = str;
                arg->type = RV2_NODE_STR;
                arg->vtype = RV2_VT_STR;
            }
            vt = rv2_check_value(p, arg, strchr("nif", func->args[a]) != NULL);
            if (!vt)
                return 0;
            if (func->args[a] == 's' && vt != RV2_VT_STR) {
                rv2_error(p, arg->start, "%s() argument %d must be a string, found a %s", func->name, a + 1, rv2VtNames[vt]);
                return 0;
            }
            if (func->args[a] == 'a' && vt != RV2_VT_IP) {
                rv2_error(p, arg->start, "%s() argument %d must be an ip, found a %s", func->name, a + 1, rv2VtNames[vt]);
                return 0;
            }
            if (func->args[a] == 'i' && vt != RV2_VT_INT) {
                rv2_error(p, arg->start, "%s() argument %d must be an integer, found a %s", func->name, a + 1, rv2VtNames[vt]);
                return 0;
            }
            if (func->args[a] == 'f' && !rv2_is_numeric(vt)) {
                rv2_error(p, arg->start, "%s() argument %d must be a number, found a %s", func->name, a + 1, rv2VtNames[vt]);
                return 0;
            }
            if (func->args[a] == 'n') {
                if (!rv2_is_numeric(vt)) {
                    rv2_error(p, arg->start, "%s() argument %d must be a number, found a %s", func->name, a + 1, rv2VtNames[vt]);
                    return 0;
                }
                if (arg->multi) {
                    rv2_error(p, arg->start, "%s() needs a single value, use count() for the number of values", func->name);
                    return 0;
                }
                if (vt == RV2_VT_FLOAT)
                    numVt = RV2_VT_FLOAT;
            }
            if ((func->flags & RV2_FUNC_PER_VALUE) && arg->multi)
                node->multi = 1;
            break;
        }
        continue;

needField:
        if (!p->error)
            rv2_error(p, arg->start, "%s() argument %d must be a field", func->name, a + 1);
        return 0;
    }

    // Same as bpf: rules, ok if it compiles for any supported DLT
    if (strcmp(func->name, "bpf") == 0) {
        char error[PCAP_ERRBUF_SIZE] = "";
        int attempted = 0, compiled = 0;
        for (int d = 0; d < arkime_packet_dlt_index_count(); d++) {
            const int dlt = arkime_packet_index_to_dlt(d);
            if (dlt == DLT_NFLOG)
                continue;
            attempted++;
            struct bpf_program bpfp;
            pcap_t *deadPcap = pcap_open_dead(dlt, 65535);
            if (pcap_compile(deadPcap, &bpfp, RV2_KID(node, 0)->str, 1, PCAP_NETMASK_UNKNOWN) == -1) {
                if (!error[0])
                    g_strlcpy(error, pcap_geterr(deadPcap), sizeof(error));
            } else {
                compiled++;
                pcap_freecode(&bpfp);
            }
            pcap_close(deadPcap);
        }
        if (attempted && !compiled) {
            rv2_error(p, RV2_KID(node, 0)->start, "bad bpf: %s", error);
            return 0;
        }
    }

    node->vtype = func->ret ? func->ret : numVt;
    return node->vtype;
}
/******************************************************************************/
LOCAL int rv2_check(RV2Parser_t *p, ArkimeRulesV2Node_t *node)
{
    switch (node->type) {
    case RV2_NODE_AND:
    case RV2_NODE_OR:
    case RV2_NODE_NOT:
        for (int k = 0; k < RV2_NKIDS(node); k++) {
            if (!rv2_check_bool(p, RV2_KID(node, k)))
                return 0;
        }
        node->vtype = RV2_VT_BOOL;
        return node->vtype;

    case RV2_NODE_CMP:
        return rv2_check_cmp(p, node);

    case RV2_NODE_ARITH:
        return rv2_check_arith(p, node);

    case RV2_NODE_NEG: {
        ArkimeRulesV2Node_t *kid = RV2_KID(node, 0);
        const int vt = rv2_check_value(p, kid, TRUE);
        if (!vt)
            return 0;
        if (!rv2_is_numeric(vt) || kid->multi) {
            rv2_error(p, node->start, "'-' needs a single number");
            return 0;
        }
        node->vtype = vt;
        return node->vtype;
    }

    case RV2_NODE_CALL:
        return rv2_check_call(p, node);

    case RV2_NODE_WORD:
        return rv2_resolve_word(p, node, FALSE);

    case RV2_NODE_QUOTED: {
        char *str = rv2_unquote(node->str, strlen(node->str));
        g_free(node->str);
        node->str = str;
        node->type = RV2_NODE_STR;
        node->vtype = RV2_VT_STR;
        return node->vtype;
    }

    case RV2_NODE_REGEX:
    case RV2_NODE_LIST:
    case RV2_NODE_EXISTS_TOK:
        rv2_error(p, node->start, "'%s' can only be used on the right side of == or !=", node->str);
        return 0;

    default:
        return node->vtype;
    }
}
/******************************************************************************/
LOCAL void rv2_print_str(GString *out, const char *str)
{
    g_string_append_c(out, '"');
    for (; *str; str++) {
        if (*str == '"' || *str == '\\')
            g_string_append_c(out, '\\');
        g_string_append_c(out, *str);
    }
    g_string_append_c(out, '"');
}
/******************************************************************************/
LOCAL void rv2_print_value(GString *out, const RV2Value_t *value)
{
    char ipstr[INET6_ADDRSTRLEN];

    switch (value->kind) {
    case RV2_VAL_INT:
        g_string_append_printf(out, "%" PRId64, value->imin);
        break;
    case RV2_VAL_INT_RANGE:
        g_string_append_printf(out, "range:%" PRId64 "-%" PRId64, value->imin, value->imax);
        break;
    case RV2_VAL_FLOAT:
        g_string_append_printf(out, "%g", value->fmin);
        break;
    case RV2_VAL_FLOAT_RANGE:
        g_string_append_printf(out, "range:%g-%g", value->fmin, value->fmax);
        break;
    case RV2_VAL_STR:
        rv2_print_str(out, value->str);
        break;
    case RV2_VAL_HEAD:
        g_string_append(out, "head:");
        rv2_print_str(out, value->str);
        break;
    case RV2_VAL_TAIL:
        g_string_append(out, "tail:");
        rv2_print_str(out, value->str);
        break;
    case RV2_VAL_CONTAINS:
        g_string_append(out, "contains:");
        rv2_print_str(out, value->str);
        break;
    case RV2_VAL_GLOB:
        g_string_append(out, "glob:");
        rv2_print_str(out, value->str);
        break;
    case RV2_VAL_REGEX:
        g_string_append_printf(out, "regex:/%s/", value->str);
        break;
    case RV2_VAL_CIDR:
        if (value->family == 4) {
            if (value->bits == 0 && !memcmp(value->ip.s6_addr + 12, "\0\0\0\0", 4))
                g_string_append(out, "ipv4");
            else
                g_string_append_printf(out, "%s/%u", inet_ntop(AF_INET, value->ip.s6_addr + 12, ipstr, sizeof(ipstr)), value->bits);
        } else {
            if (value->bits == 0 && IN6_IS_ADDR_UNSPECIFIED(&value->ip))
                g_string_append(out, "ipv6");
            else
                g_string_append_printf(out, "%s/%u", inet_ntop(AF_INET6, &value->ip, ipstr, sizeof(ipstr)), value->bits);
        }
        break;
    }
}
/******************************************************************************/
void arkime_rulesv2_print(const ArkimeRulesV2Node_t *node, GString *out)
{
    switch (node->type) {
    case RV2_NODE_FIELD:
        g_string_append(out, config.fields[node->field]->expression);
        return;
    case RV2_NODE_PLACEHOLDER:
        g_string_append_c(out, '_');
        return;
    case RV2_NODE_INT:
        g_string_append_printf(out, "%" PRId64, node->i);
        return;
    case RV2_NODE_FLOAT:
        g_string_append_printf(out, "%g", node->f);
        return;
    case RV2_NODE_STR:
        rv2_print_str(out, node->str);
        return;
    case RV2_NODE_BOOL:
        g_string_append(out, node->i ? "true" : "false");
        return;
    case RV2_NODE_WORD:
    case RV2_NODE_QUOTED:
    case RV2_NODE_REGEX:
    case RV2_NODE_LIST:
    case RV2_NODE_EXISTS_TOK:
        g_string_append(out, node->str);
        return;
    default:
        break;
    }

    g_string_append_c(out, '(');
    switch (node->type) {
    case RV2_NODE_AND:
        g_string_append(out, "and");
        break;
    case RV2_NODE_OR:
        g_string_append(out, "or");
        break;
    case RV2_NODE_NOT:
        g_string_append(out, "not");
        break;
    case RV2_NODE_NEG:
        g_string_append(out, "neg");
        break;
    case RV2_NODE_EXISTS:
        g_string_append(out, node->op == RV2_TOK_EQ ? "exists" : "!exists");
        break;
    case RV2_NODE_MATCH:
        g_string_append(out, rv2_op_str(node->op));
        if (node->andList)
            g_string_append(out, "all");
        break;
    case RV2_NODE_CALL:
        g_string_append(out, node->str);
        break;
    default:
        g_string_append(out, rv2_op_str(node->op));
        break;
    }

    for (int k = 0; k < RV2_NKIDS(node); k++) {
        g_string_append_c(out, ' ');
        arkime_rulesv2_print(RV2_KID(node, k), out);
    }

    for (int v = 0; v < node->nvalues; v++) {
        g_string_append_c(out, ' ');
        rv2_print_value(out, &node->values[v]);
    }
    g_string_append_c(out, ')');
}
/******************************************************************************/
// Frees p->error and returns it with the line, column and a caret under the problem
LOCAL char *rv2_format_error(RV2Parser_t *p)
{
    int line = 1;
    int lineStart = 0;
    for (int i = 0; i < p->errorPos && i < p->len; i++) {
        if (p->src[i] == '\n') {
            line++;
            lineStart = i + 1;
        }
    }
    int lineEnd = lineStart;
    while (lineEnd < p->len && p->src[lineEnd] != '\n')
        lineEnd++;

    char *error = g_strdup_printf("%s at line %d col %d\n    %.*s\n    %*s^",
                                  p->error, line, p->errorPos - lineStart + 1,
                                  lineEnd - lineStart, p->src + lineStart,
                                  p->errorPos - lineStart, "");
    g_free(p->error);
    p->error = NULL;
    return error;
}
/******************************************************************************/
/* Parse and type check an expression. On failure returns NULL and sets *error
 * to a message with the line, column and a caret under the problem. */
ArkimeRulesV2Node_t *arkime_rulesv2_parse(const char *expression, int ruleType, char **error)
{
    RV2Parser_t p;
    memset(&p, 0, sizeof(p));
    p.src = expression;
    p.len = strlen(expression);
    p.ruleType = ruleType;

    ArkimeRulesV2Node_t *root = rv2_parse_expr(&p, RV2_PREC_OR);
    if (root) {
        RV2Tok_t tok = rv2_peek(&p, FALSE);
        if (tok.type != RV2_TOK_EOF)
            rv2_error_tok(&p, &tok, "expected '&&', '||' or end of expression");
        else
            rv2_check_bool(&p, root);
    }

    if (!p.error)
        return root;

    arkime_rulesv2_free(root);
    *error = rv2_format_error(&p);
    return NULL;
}
/******************************************************************************/
/* Lowering, rewrites the checked tree so codegen only sees
 *  - == and never !=, a != b becomes !(a == b) so a missing field matches !=
 *  - ]a,b[ lists as an && of single item matches
 *  - multi value fields only as the subject of a match, exists, count(),
 *    has(), any() or all(), anywhere else the leaf is wrapped in any()
 */
/******************************************************************************/
LOCAL gboolean rv2_is_call(const ArkimeRulesV2Node_t *node, const char *name)
{
    return node->type == RV2_NODE_CALL && strcmp(node->func->name, name) == 0;
}
/******************************************************************************/
LOCAL ArkimeRulesV2Node_t *rv2_wrap_not(ArkimeRulesV2Node_t *node)
{
    ArkimeRulesV2Node_t *not = rv2_node_new(RV2_NODE_NOT, node->start, node->end);
    not->vtype = RV2_VT_BOOL;
    rv2_node_add_kid(not, node);
    return not;
}
/******************************************************************************/
// Deep copy of a value expression
LOCAL ArkimeRulesV2Node_t *rv2_node_clone(const ArkimeRulesV2Node_t *node)
{
    ArkimeRulesV2Node_t *clone = g_new(ArkimeRulesV2Node_t, 1);
    *clone = *node;
    clone->str = g_strdup(node->str);
    clone->kids = NULL;
    clone->values = NULL;
    clone->nvalues = 0;
    for (int k = 0; k < RV2_NKIDS(node); k++)
        rv2_node_add_kid(clone, rv2_node_clone(RV2_KID(node, k)));
    return clone;
}
/******************************************************************************/
// Fields that are used directly and not as one value
LOCAL gboolean rv2_direct_field(const ArkimeRulesV2Node_t *parent, int k)
{
    if (k != 0)
        return FALSE;
    if (parent->type == RV2_NODE_MATCH)
        return TRUE;
    return rv2_is_call(parent, "count") || rv2_is_call(parent, "has") || rv2_is_call(parent, "any") || rv2_is_call(parent, "all");
}
/******************************************************************************/
LOCAL ArkimeRulesV2Node_t **rv2_find_multi(ArkimeRulesV2Node_t *node)
{
    for (int k = 0; k < RV2_NKIDS(node); k++) {
        ArkimeRulesV2Node_t **slot = (ArkimeRulesV2Node_t **)&g_ptr_array_index(node->kids, k);
        if ((*slot)->type == RV2_NODE_FIELD) {
            if ((*slot)->multi && !rv2_direct_field(node, k))
                return slot;
            continue;
        }
        slot = rv2_find_multi(*slot);
        if (slot)
            return slot;
    }
    return NULL;
}
/******************************************************************************/
LOCAL ArkimeRulesV2Node_t *rv2_wrap_multi(RV2Parser_t *p, ArkimeRulesV2Node_t *leaf, int depth)
{
    ArkimeRulesV2Node_t **slot = rv2_find_multi(leaf);
    if (!slot)
        return leaf;

    ArkimeRulesV2Node_t *field = *slot;
    if (depth >= RV2_MAX_DEPTH) {
        rv2_error(p, field->start, "too many multi value fields in one comparison");
        return leaf;
    }

    ArkimeRulesV2Node_t *placeholder = rv2_node_new(RV2_NODE_PLACEHOLDER, field->start, field->end);
    placeholder->vtype = field->vtype;
    placeholder->i = depth;
    *slot = placeholder;

    ArkimeRulesV2Node_t *any = rv2_node_new(RV2_NODE_CALL, leaf->start, leaf->end);
    any->str = g_strdup("any");
    any->func = rv2_lookup_func("any");
    any->vtype = RV2_VT_BOOL;
    any->i = depth;
    rv2_node_add_kid(any, field);
    rv2_node_add_kid(any, rv2_wrap_multi(p, leaf, depth + 1));
    return any;
}
/******************************************************************************/
LOCAL ArkimeRulesV2Node_t *rv2_lower(RV2Parser_t *p, ArkimeRulesV2Node_t *node, int depth)
{
    switch (node->type) {
    case RV2_NODE_AND:
    case RV2_NODE_OR:
    case RV2_NODE_NOT:
        for (int k = 0; k < RV2_NKIDS(node); k++)
            g_ptr_array_index(node->kids, k) = rv2_lower(p, RV2_KID(node, k), depth);
        return node;

    case RV2_NODE_EXISTS:
        if (node->op == RV2_TOK_NE) {
            node->op = RV2_TOK_EQ;
            node->textNeg = 1;
            return rv2_wrap_not(node);
        }
        return node;

    case RV2_NODE_MATCH: {
        const gboolean ne = node->op == RV2_TOK_NE;
        node->op = RV2_TOK_EQ;

        if (node->andList && node->nvalues > 1) {
            ArkimeRulesV2Node_t *and = rv2_node_new(RV2_NODE_AND, node->start, node->end);
            and->vtype = RV2_VT_BOOL;
            for (int v = 0; v < node->nvalues; v++) {
                ArkimeRulesV2Node_t *match = rv2_node_new(RV2_NODE_MATCH, node->start, node->end);
                match->op = RV2_TOK_EQ;
                match->vtype = RV2_VT_BOOL;
                rv2_node_add_kid(match, rv2_node_clone(RV2_KID(node, 0)));
                match->values = g_new(RV2Value_t, 1);
                match->values[0] = node->values[v];
                match->nvalues = 1;
                match->textNeg = ne;
                rv2_node_add_kid(and, rv2_lower(p, match, depth));
            }
            node->nvalues = 0;
            arkime_rulesv2_free(node);
            return ne ? rv2_wrap_not(and) : and;
        }

        node->andList = 0;
        if (RV2_KID(node, 0)->type != RV2_NODE_FIELD)
            node = rv2_wrap_multi(p, node, depth);
        node->textNeg = ne;
        return ne ? rv2_wrap_not(node) : node;
    }

    case RV2_NODE_CMP: {
        const gboolean ne = node->op == RV2_TOK_NE;
        if (ne)
            node->op = RV2_TOK_EQ;
        node = rv2_wrap_multi(p, node, depth);
        node->textNeg = ne;
        return ne ? rv2_wrap_not(node) : node;
    }

    case RV2_NODE_CALL:
        if (rv2_is_call(node, "any") || rv2_is_call(node, "all")) {
            node->i = depth;
            g_ptr_array_index(node->kids, 1) = rv2_lower(p, RV2_KID(node, 1), depth + 1);
            return node;
        }
        if (rv2_is_call(node, "has") || rv2_is_call(node, "bpf"))
            return node;
        return rv2_wrap_multi(p, node, depth);

    default:
        return node;
    }
}
/******************************************************************************/
/* Program */
/******************************************************************************/
typedef enum {
    RV2_K_NONE,
    RV2_K_INT,
    RV2_K_FLOAT,
    RV2_K_STR,
    RV2_K_INTSET,
    RV2_K_FLOATSET,
    RV2_K_STRSET,
    RV2_K_IPSET,
    RV2_K_BPF,
    RV2_K_CALL
} RV2ConstKind;

typedef struct {
    char            *str;
    int              len;
} RV2Str_t;

typedef struct {
    int64_t         *vals;          // sorted, binary searched when there are only a few
    int              nvals;
    int64_t         *ranges;        // min, max pairs
    int              nranges;
    uint32_t         hashMask;      // open addressing table when there are many vals
    int64_t         *hashKeys;
    uint8_t         *hashUsed;
} RV2IntSet_t;

typedef struct {
    double          *vals;          // compared at float precision, the field storage size
    int              nvals;
    double          *ranges;        // min, max pairs
    int              nranges;
} RV2FloatSet_t;

typedef struct {
    gboolean         fewExact;      // only a few exact values, matched inline
    gboolean         noPatterns;    // no hash, glob or regex, uses the smaller matcher
    gboolean         hashOnly;      // only exact values, enough for the hash
    gboolean         tailOnly;      // only endsWith values, matched inline
    uint32_t         hashMask;      // open addressing table of the exact values when there are more than a few
    uint32_t        *hashCodes;
    const char     **hashStrs;      // NULL is an empty slot
    RV2Str_t        *exact;
    int              nexact;
    RV2Str_t        *head;
    int              nhead;
    RV2Str_t        *tail;
    int              ntail;
    RV2Str_t        *contains;
    int              ncontains;
    char           **globs;
    int              nglobs;
    GRegex         **regexes;
    int              nregexes;
} RV2StrSet_t;

typedef struct {
    patricia_tree_t *tree4;
    patricia_tree_t *tree6;
} RV2IpSet_t;

typedef struct {
    char               *filter;
    struct bpf_program *bpfp[ARKIME_DLT_MAX];   // per supported DLT, atomically replaced by recompile
} RV2Bpf_t;

typedef struct {
    const RV2Func_t    *func;
    int                 nargs;
    uint8_t             regs[RV2_MAX_ARGS];      // arg registers
} RV2Call_t;

typedef struct {
    RV2ConstKind     kind;
    union {
        int64_t        i;
        double         f;
        RV2Str_t       s;
        RV2IntSet_t   *iset;
        RV2FloatSet_t *fset;
        RV2StrSet_t   *sset;
        RV2IpSet_t    *ipset;
        RV2Bpf_t      *bpf;
        RV2Call_t     *call;
    };
} RV2Const_t;

#define RV2_F_RET   0x01    // ends the block
#define RV2_F_TEST  0x02    // jumps to jt or jf
#define RV2_F_MISS  0x04    // continues, or jumps to jf when the value is missing
#define RV2_F_FIELD 0x08    // uses field
#define RV2_F_SUB   0x10    // runs block k for each field value, placeholder in register a, b is the value type
#define RV2_F_CALL  0x20    // calls a registered function, k has it and the arg registers

//   name               flags                                  dst    a      b      const
#define RV2_OPS(X) \
    X(RET_TRUE,         RV2_F_RET,                             NONE,  NONE,  NONE,  NONE) \
    X(RET_FALSE,        RV2_F_RET,                             NONE,  NONE,  NONE,  NONE) \
    X(LOADK_INT,        0,                                     INT,   NONE,  NONE,  INT) \
    X(LOADK_FLOAT,      0,                                     FLOAT, NONE,  NONE,  FLOAT) \
    X(LOADK_STR,        0,                                     STR,   NONE,  NONE,  STR) \
    X(LOAD_INT,         RV2_F_MISS | RV2_F_FIELD,              INT,   NONE,  NONE,  NONE) \
    X(LOAD_FLOAT,       RV2_F_MISS | RV2_F_FIELD,              FLOAT, NONE,  NONE,  NONE) \
    X(LOAD_STR,         RV2_F_MISS | RV2_F_FIELD,              STR,   NONE,  NONE,  NONE) \
    X(LOAD_IP,          RV2_F_MISS | RV2_F_FIELD,              IP,    NONE,  NONE,  NONE) \
    X(COUNT,            RV2_F_FIELD,                           INT,   NONE,  NONE,  NONE) \
    X(I2F,              0,                                     FLOAT, INT,   NONE,  NONE) \
    X(ADD_I,            0,                                     INT,   INT,   INT,   NONE) \
    X(SUB_I,            0,                                     INT,   INT,   INT,   NONE) \
    X(MUL_I,            0,                                     INT,   INT,   INT,   NONE) \
    X(DIV_I,            RV2_F_MISS,                            INT,   INT,   INT,   NONE) \
    X(MOD_I,            RV2_F_MISS,                            INT,   INT,   INT,   NONE) \
    X(NEG_I,            0,                                     INT,   INT,   NONE,  NONE) \
    X(ABS_I,            0,                                     INT,   INT,   NONE,  NONE) \
    X(MIN_I,            0,                                     INT,   INT,   INT,   NONE) \
    X(MAX_I,            0,                                     INT,   INT,   INT,   NONE) \
    X(ADD_F,            0,                                     FLOAT, FLOAT, FLOAT, NONE) \
    X(SUB_F,            0,                                     FLOAT, FLOAT, FLOAT, NONE) \
    X(MUL_F,            0,                                     FLOAT, FLOAT, FLOAT, NONE) \
    X(DIV_F,            0,                                     FLOAT, FLOAT, FLOAT, NONE) \
    X(NEG_F,            0,                                     FLOAT, FLOAT, NONE,  NONE) \
    X(ABS_F,            0,                                     FLOAT, FLOAT, NONE,  NONE) \
    X(MIN_F,            0,                                     FLOAT, FLOAT, FLOAT, NONE) \
    X(MAX_F,            0,                                     FLOAT, FLOAT, FLOAT, NONE) \
    X(LEN,              0,                                     INT,   STR,   NONE,  NONE) \
    X(LOWER,            0,                                     STR,   STR,   NONE,  NONE) \
    X(UPPER,            0,                                     STR,   STR,   NONE,  NONE) \
    X(EQ_I,             RV2_F_TEST,                            NONE,  INT,   INT,   NONE) \
    X(LT_I,             RV2_F_TEST,                            NONE,  INT,   INT,   NONE) \
    X(LE_I,             RV2_F_TEST,                            NONE,  INT,   INT,   NONE) \
    X(GT_I,             RV2_F_TEST,                            NONE,  INT,   INT,   NONE) \
    X(GE_I,             RV2_F_TEST,                            NONE,  INT,   INT,   NONE) \
    X(EQ_F,             RV2_F_TEST,                            NONE,  FLOAT, FLOAT, NONE) \
    X(LT_F,             RV2_F_TEST,                            NONE,  FLOAT, FLOAT, NONE) \
    X(LE_F,             RV2_F_TEST,                            NONE,  FLOAT, FLOAT, NONE) \
    X(GT_F,             RV2_F_TEST,                            NONE,  FLOAT, FLOAT, NONE) \
    X(GE_F,             RV2_F_TEST,                            NONE,  FLOAT, FLOAT, NONE) \
    X(EQ_S,             RV2_F_TEST,                            NONE,  STR,   STR,   NONE) \
    X(EQ_IP,            RV2_F_TEST,                            NONE,  IP,    IP,    NONE) \
    X(MATCH_FIELD_INT,  RV2_F_TEST | RV2_F_FIELD,              NONE,  NONE,  NONE,  INTSET) \
    X(MATCH_FIELD_FLOAT,RV2_F_TEST | RV2_F_FIELD,              NONE,  NONE,  NONE,  FLOATSET) \
    X(MATCH_FIELD_STR,  RV2_F_TEST | RV2_F_FIELD,              NONE,  NONE,  NONE,  STRSET) \
    X(MATCH_FIELD_IP,   RV2_F_TEST | RV2_F_FIELD,              NONE,  NONE,  NONE,  IPSET) \
    X(MATCH_INT,        RV2_F_TEST,                            NONE,  INT,   NONE,  INTSET) \
    X(MATCH_FLOAT,      RV2_F_TEST,                            NONE,  FLOAT, NONE,  FLOATSET) \
    X(MATCH_STR,        RV2_F_TEST,                            NONE,  STR,   NONE,  STRSET) \
    X(MATCH_IP,         RV2_F_TEST,                            NONE,  IP,    NONE,  IPSET) \
    X(EXISTS,           RV2_F_TEST | RV2_F_FIELD,              NONE,  NONE,  NONE,  NONE) \
    X(STARTS,           RV2_F_TEST,                            NONE,  STR,   NONE,  STR) \
    X(ENDS,             RV2_F_TEST,                            NONE,  STR,   NONE,  STR) \
    X(CONTAINS,         RV2_F_TEST,                            NONE,  STR,   NONE,  STR) \
    X(ICONTAINS,        RV2_F_TEST,                            NONE,  STR,   NONE,  STR) \
    X(IS_PRIVATE,       RV2_F_TEST,                            NONE,  IP,    NONE,  NONE) \
    X(IS_MULTICAST,     RV2_F_TEST,                            NONE,  IP,    NONE,  NONE) \
    X(IS_V4,            RV2_F_TEST,                            NONE,  IP,    NONE,  NONE) \
    X(IS_V6,            RV2_F_TEST,                            NONE,  IP,    NONE,  NONE) \
    X(BPF,              RV2_F_TEST,                            NONE,  NONE,  NONE,  BPF) \
    X(BOOL,             RV2_F_TEST,                            NONE,  NONE,  NONE,  NONE) \
    X(CALL_I,           RV2_F_MISS | RV2_F_CALL,               INT,   NONE,  NONE,  CALL) \
    X(CALL_F,           RV2_F_MISS | RV2_F_CALL,               FLOAT, NONE,  NONE,  CALL) \
    X(CALL_S,           RV2_F_MISS | RV2_F_CALL,               STR,   NONE,  NONE,  CALL) \
    X(CALL_IP,          RV2_F_MISS | RV2_F_CALL,               IP,    NONE,  NONE,  CALL) \
    X(CALL_B,           RV2_F_TEST | RV2_F_CALL,               NONE,  NONE,  NONE,  CALL) \
    X(ANY,              RV2_F_TEST | RV2_F_FIELD | RV2_F_SUB,  NONE,  NONE,  NONE,  NONE) \
    X(ALL,              RV2_F_TEST | RV2_F_FIELD | RV2_F_SUB,  NONE,  NONE,  NONE,  NONE)

#define RV2_OP_ENUM(name, flags, dst, a, b, k) RV2_OP_##name,
typedef enum {
    RV2_OPS(RV2_OP_ENUM)
    RV2_OP_NUM
} RV2Op;

typedef struct {
    const char *name;
    uint8_t     flags;
    uint8_t     dst;
    uint8_t     a;
    uint8_t     b;
    uint8_t     k;
} RV2OpInfo_t;

#define RV2_OP_INFO(name, flags, dst, a, b, k) {#name, flags, RV2_VT_##dst, RV2_VT_##a, RV2_VT_##b, RV2_K_##k},
LOCAL const RV2OpInfo_t rv2OpInfo[] = {
    RV2_OPS(RV2_OP_INFO)
};

typedef struct {
    uint8_t          op;
    uint8_t          dst;
    uint8_t          a;
    uint8_t          b;
    uint16_t         jt;
    uint16_t         jf;
    int16_t          field;
    uint16_t         k;
    uint32_t         unused;
} RV2Insn_t;

// Explain only, kept out of the instructions so they stay small
typedef struct {
    int              start;         // test ops, source of the condition, 0 len for none
    int              len;
    uint8_t          neg;           // condition is under !, so false is what matches
    uint8_t          textNeg;       // source was written with != so it already reads that way
} RV2Span_t;

// Jump targets that return instead of going to a RET instruction
#define RV2_JUMP_RET_FALSE 0xfffe
#define RV2_JUMP_RET_TRUE  0xffff

typedef struct {
    RV2Insn_t       *insns;
    RV2Span_t       *spans;         // one per insn
    int              ninsns;
    int              depth;                      // placeholders live at entry
    uint8_t          entryTypes[RV2_MAX_DEPTH];  // set by the verifier from the caller
} RV2Block_t;

struct arkime_rulesv2_prog {
    RV2Block_t             *blocks;
    int                     nblocks;
    RV2Const_t             *consts;
    int                     nconsts;
    int                     nregs;
    ArkimeRulesV2Trigger_t *triggers;   // fieldSet rules only
    int                     ntriggers;
    char                   *source;
    int                     sourceLen;
    gboolean                testsOnly;   // just field tests, runs on the small interpreter
    int                     quickField;  // first test's session field, when unset the answer is quickResult
    gboolean                quickResult;
    gboolean                firstTest;   // field test only program, run the first test on its own
};

typedef ArkimeRulesV2Value_t RV2Reg_t;

/******************************************************************************/
/* Matcher sets */
/******************************************************************************/
LOCAL int rv2_int64_cmp(const void *a, const void *b)
{
    const int64_t x = *(const int64_t *)a, y = *(const int64_t *)b;
    return x < y ? -1 : x > y;
}
/******************************************************************************/
/* Large sets use small open addressing tables, at most half full, instead of
 * GHashTable, so a lookup is inline with no hash or equal function calls. */
#define RV2_INTSET_HASH_MIN 16

LOCAL uint32_t rv2_hash_size(int n)
{
    uint32_t size = 16;
    while (size < (uint32_t)n * 2)
        size <<= 1;
    return size;
}
/******************************************************************************/
LOCAL inline uint32_t rv2_int_hash(int64_t v)
{
    return (uint32_t)(((uint64_t)v * 0x9E3779B97F4A7C15ULL) >> 32);
}
/******************************************************************************/
// FNV-1a
LOCAL inline uint32_t rv2_str_hash(const char *str)
{
    uint32_t h = 2166136261u;
    for (; *str; str++)
        h = (h ^ (uint8_t)*str) * 16777619u;
    return h;
}
/******************************************************************************/
LOCAL inline gboolean rv2_strhash_contains(const RV2StrSet_t *set, const char *str)
{
    const uint32_t code = rv2_str_hash(str);
    for (uint32_t slot = code & set->hashMask; set->hashStrs[slot]; slot = (slot + 1) & set->hashMask) {
        if (set->hashCodes[slot] == code && strcmp(set->hashStrs[slot], str) == 0)
            return TRUE;
    }
    return FALSE;
}
/******************************************************************************/
LOCAL RV2IntSet_t *rv2_intset_new(const RV2Value_t *values, int nvalues)
{
    RV2IntSet_t *set = g_new0(RV2IntSet_t, 1);
    GArray *vals = g_array_new(FALSE, FALSE, sizeof(int64_t));
    GArray *ranges = g_array_new(FALSE, FALSE, sizeof(int64_t));

    for (int v = 0; v < nvalues; v++) {
        // Small ranges are cheaper as values
        if (values[v].kind == RV2_VAL_INT_RANGE && values[v].imax - values[v].imin >= 20) {
            g_array_append_val(ranges, values[v].imin);
            g_array_append_val(ranges, values[v].imax);
            continue;
        }
        for (int64_t i = values[v].imin; i <= values[v].imax; i++)
            g_array_append_val(vals, i);
    }

    g_array_sort(vals, rv2_int64_cmp);
    int n = 0;
    for (guint i = 0; i < vals->len; i++) {
        if (n == 0 || g_array_index(vals, int64_t, n - 1) != g_array_index(vals, int64_t, i))
            g_array_index(vals, int64_t, n++) = g_array_index(vals, int64_t, i);
    }
    set->nvals = n;
    set->vals = (int64_t *)g_array_free(vals, FALSE);
    set->nranges = ranges->len / 2;
    set->ranges = (int64_t *)g_array_free(ranges, FALSE);

    if (set->nvals > RV2_INTSET_HASH_MIN) {
        const uint32_t size = rv2_hash_size(set->nvals);
        set->hashMask = size - 1;
        set->hashKeys = g_new(int64_t, size);
        set->hashUsed = g_new0(uint8_t, size);
        for (int i = 0; i < set->nvals; i++) {
            uint32_t slot = rv2_int_hash(set->vals[i]) & set->hashMask;
            while (set->hashUsed[slot])
                slot = (slot + 1) & set->hashMask;
            set->hashUsed[slot] = 1;
            set->hashKeys[slot] = set->vals[i];
        }
    }
    return set;
}
/******************************************************************************/
LOCAL inline gboolean rv2_intset_match(const RV2IntSet_t *set, int64_t v)
{
    if (set->hashKeys) {
        for (uint32_t slot = rv2_int_hash(v) & set->hashMask; set->hashUsed[slot]; slot = (slot + 1) & set->hashMask) {
            if (set->hashKeys[slot] == v)
                return TRUE;
        }
        goto ranges;
    }

    int lo = 0, hi = set->nvals - 1;
    while (lo <= hi) {
        const int mid = (lo + hi) / 2;
        if (set->vals[mid] == v)
            return TRUE;
        if (set->vals[mid] < v)
            lo = mid + 1;
        else
            hi = mid - 1;
    }

ranges:
    for (int r = 0; r < set->nranges; r++) {
        if (v >= set->ranges[r * 2] && v <= set->ranges[r * 2 + 1])
            return TRUE;
    }
    return FALSE;
}
/******************************************************************************/
LOCAL RV2FloatSet_t *rv2_floatset_new(const RV2Value_t *values, int nvalues)
{
    RV2FloatSet_t *set = g_new0(RV2FloatSet_t, 1);
    set->vals = g_new(double, nvalues);
    set->ranges = g_new(double, nvalues * 2);
    for (int v = 0; v < nvalues; v++) {
        if (values[v].kind == RV2_VAL_FLOAT_RANGE) {
            set->ranges[set->nranges * 2] = values[v].fmin;
            set->ranges[set->nranges * 2 + 1] = values[v].fmax;
            set->nranges++;
        } else {
            set->vals[set->nvals++] = values[v].fmin;
        }
    }
    return set;
}
/******************************************************************************/
LOCAL inline gboolean rv2_floatset_match(const RV2FloatSet_t *set, double v)
{
    for (int i = 0; i < set->nvals; i++) {
        if ((float)v == (float)set->vals[i])
            return TRUE;
    }
    for (int r = 0; r < set->nranges; r++) {
        if (v >= set->ranges[r * 2] && v <= set->ranges[r * 2 + 1])
            return TRUE;
    }
    return FALSE;
}
/******************************************************************************/
LOCAL void rv2_str_add(RV2Str_t **array, int *n, int size, const RV2Value_t *value)
{
    if (!*array)
        *array = g_new0(RV2Str_t, size);
    (*array)[*n].str = g_strdup(value->str);
    (*array)[*n].len = value->len;
    (*n)++;
}
/******************************************************************************/
#define RV2_STRSET_HASH_MIN 5
LOCAL RV2StrSet_t *rv2_strset_new(RV2Value_t *values, int nvalues)
{
    RV2StrSet_t *set = g_new0(RV2StrSet_t, 1);

    for (int v = 0; v < nvalues; v++) {
        RV2Value_t *value = &values[v];
        switch (value->kind) {
        case RV2_VAL_STR:
            rv2_str_add(&set->exact, &set->nexact, nvalues, value);
            break;
        case RV2_VAL_HEAD:
            rv2_str_add(&set->head, &set->nhead, nvalues, value);
            break;
        case RV2_VAL_TAIL:
            rv2_str_add(&set->tail, &set->ntail, nvalues, value);
            break;
        case RV2_VAL_CONTAINS:
            rv2_str_add(&set->contains, &set->ncontains, nvalues, value);
            break;
        case RV2_VAL_GLOB:
            if (!set->globs)
                set->globs = g_new0(char *, nvalues);
            set->globs[set->nglobs++] = g_strdup(value->str);
            break;
        case RV2_VAL_REGEX:
            if (!set->regexes)
                set->regexes = g_new0(GRegex *, nvalues);
            set->regexes[set->nregexes++] = g_regex_ref(value->regex);
            break;
        default:
            break;
        }
    }

    if (set->nexact >= RV2_STRSET_HASH_MIN) {
        const uint32_t size = rv2_hash_size(set->nexact);
        set->hashMask = size - 1;
        set->hashCodes = g_new(uint32_t, size);
        set->hashStrs = g_new0(const char *, size);
        for (int i = 0; i < set->nexact; i++) {
            const uint32_t code = rv2_str_hash(set->exact[i].str);
            uint32_t slot = code & set->hashMask;
            gboolean dup = FALSE;
            for (; set->hashStrs[slot]; slot = (slot + 1) & set->hashMask) {
                if (set->hashCodes[slot] == code && strcmp(set->hashStrs[slot], set->exact[i].str) == 0) {
                    dup = TRUE;
                    break;
                }
            }
            if (!dup) {
                set->hashCodes[slot] = code;
                set->hashStrs[slot] = set->exact[i].str;
            }
        }
    }
    set->noPatterns = !set->hashStrs && !set->nglobs && !set->nregexes;
    set->hashOnly = set->hashStrs && !set->nhead && !set->ntail && !set->ncontains && !set->nglobs && !set->nregexes;
    set->tailOnly = set->noPatterns && !set->nexact && !set->nhead && !set->ncontains && set->ntail;
    set->fewExact = set->noPatterns && !set->nhead && !set->ntail && !set->ncontains;
    return set;
}
/******************************************************************************/
// Shell style * and ? match, backtracks only to the last *
LOCAL gboolean rv2_glob_match(const char *pat, const char *str)
{
    const char *star = NULL;
    const char *starStr = NULL;

    while (*str) {
        if (*pat == '*') {
            star = pat++;
            starStr = str;
        } else if (*pat == '?' || *pat == *str) {
            pat++;
            str++;
        } else if (star) {
            pat = star + 1;
            str = ++starStr;
        } else {
            return FALSE;
        }
    }
    while (*pat == '*')
        pat++;
    return *pat == 0;
}
/******************************************************************************/
LOCAL gboolean rv2_strset_match(const RV2StrSet_t *set, const char *str)
{
    if (set->hashStrs) {
        if (rv2_strhash_contains(set, str))
            return TRUE;
    } else {
        for (int i = 0; i < set->nexact; i++) {
            if (strcmp(set->exact[i].str, str) == 0)
                return TRUE;
        }
    }

    if (set->nhead || set->ntail || set->ncontains) {
        const int len = strlen(str);
        for (int i = 0; i < set->nhead; i++) {
            if (len >= set->head[i].len && memcmp(str, set->head[i].str, set->head[i].len) == 0)
                return TRUE;
        }
        for (int i = 0; i < set->ntail; i++) {
            if (len >= set->tail[i].len && memcmp(str + len - set->tail[i].len, set->tail[i].str, set->tail[i].len) == 0)
                return TRUE;
        }
        for (int i = 0; i < set->ncontains; i++) {
            if (arkime_memstr(str, len, set->contains[i].str, set->contains[i].len))
                return TRUE;
        }
    }

    for (int i = 0; i < set->nglobs; i++) {
        if (rv2_glob_match(set->globs[i], str))
            return TRUE;
    }
    for (int i = 0; i < set->nregexes; i++) {
        if (g_regex_match(set->regexes[i], str, 0, NULL))
            return TRUE;
    }
    return FALSE;
}
/******************************************************************************/
LOCAL RV2IpSet_t *rv2_ipset_new(const RV2Value_t *values, int nvalues)
{
    RV2IpSet_t *set = g_new0(RV2IpSet_t, 1);
    set->tree4 = New_Patricia(32);
    set->tree6 = New_Patricia(128);

    for (int v = 0; v < nvalues; v++) {
        char str[INET6_ADDRSTRLEN + 5];
        if (values[v].family == 4) {
            inet_ntop(AF_INET, values[v].ip.s6_addr + 12, str, INET6_ADDRSTRLEN);
            snprintf(str + strlen(str), 5, "/%u", values[v].bits);
            make_and_lookup(set->tree4, str);
        } else {
            inet_ntop(AF_INET6, &values[v].ip, str, INET6_ADDRSTRLEN);
            snprintf(str + strlen(str), 5, "/%u", values[v].bits);
            make_and_lookup(set->tree6, str);
        }
    }
    return set;
}
/******************************************************************************/
LOCAL inline gboolean rv2_ipset_match(const RV2IpSet_t *set, const struct in6_addr *ip)
{
    if (IN6_IS_ADDR_V4MAPPED(ip))
        return patricia_search_best3(set->tree4, ip->s6_addr + 12, 32) != NULL;
    return patricia_search_best3(set->tree6, ip->s6_addr, 128) != NULL;
}
/******************************************************************************/
LOCAL void rv2_str_array_free(RV2Str_t *array, int n)
{
    for (int i = 0; i < n; i++)
        g_free(array[i].str);
    g_free(array);
}
/******************************************************************************/
LOCAL void rv2_const_free(RV2Const_t *k)
{
    switch (k->kind) {
    case RV2_K_STR:
        g_free(k->s.str);
        break;
    case RV2_K_INTSET:
        g_free(k->iset->vals);
        g_free(k->iset->ranges);
        g_free(k->iset->hashKeys);
        g_free(k->iset->hashUsed);
        g_free(k->iset);
        break;
    case RV2_K_FLOATSET:
        g_free(k->fset->vals);
        g_free(k->fset->ranges);
        g_free(k->fset);
        break;
    case RV2_K_STRSET:
        g_free(k->sset->hashCodes);
        g_free(k->sset->hashStrs);
        rv2_str_array_free(k->sset->exact, k->sset->nexact);
        rv2_str_array_free(k->sset->head, k->sset->nhead);
        rv2_str_array_free(k->sset->tail, k->sset->ntail);
        rv2_str_array_free(k->sset->contains, k->sset->ncontains);
        for (int i = 0; i < k->sset->nglobs; i++)
            g_free(k->sset->globs[i]);
        g_free(k->sset->globs);
        for (int i = 0; i < k->sset->nregexes; i++)
            g_regex_unref(k->sset->regexes[i]);
        g_free(k->sset->regexes);
        g_free(k->sset);
        break;
    case RV2_K_IPSET:
        Destroy_Patricia(k->ipset->tree4, NULL);
        Destroy_Patricia(k->ipset->tree6, NULL);
        g_free(k->ipset);
        break;
    case RV2_K_BPF:
        for (int d = 0; d < ARKIME_DLT_MAX; d++) {
            if (k->bpf->bpfp[d])
                arkime_rules_bpfp_free(k->bpf->bpfp[d]);
        }
        g_free(k->bpf->filter);
        g_free(k->bpf);
        break;
    case RV2_K_CALL:
        g_free(k->call);
        break;
    default:
        break;
    }
}
/******************************************************************************/
void arkime_rulesv2_prog_free(ArkimeRulesV2Prog_t *prog)
{
    if (!prog)
        return;
    for (int b = 0; b < prog->nblocks; b++) {
        g_free(prog->blocks[b].insns);
        g_free(prog->blocks[b].spans);
    }
    g_free(prog->blocks);
    for (int k = 0; k < prog->nconsts; k++)
        rv2_const_free(&prog->consts[k]);
    g_free(prog->consts);
    for (int t = 0; t < prog->ntriggers; t++) {
        if (prog->triggers[t].kind >= ARKIME_RULESV2_TRIGGER_STR)
            g_free(prog->triggers[t].str);
    }
    g_free(prog->triggers);
    g_free(prog->source);
    g_free(prog);
}
/******************************************************************************/
/* Optimizer, runs on the lowered tree. Constant folding and boolean
 * simplification, merging matches on the same field into one set lookup, and
 * ordering && || children cheapest first. Nothing has side effects, so
 * children can be dropped, merged and reordered freely. */
/******************************************************************************/
LOCAL gboolean rv2_icontains(const char *str, const char *needle, int needleLen);

LOCAL gboolean rv2_value_equal(const RV2Value_t *a, const RV2Value_t *b)
{
    if (a->kind != b->kind)
        return FALSE;
    switch (a->kind) {
    case RV2_VAL_INT:
    case RV2_VAL_INT_RANGE:
        return a->imin == b->imin && a->imax == b->imax;
    case RV2_VAL_FLOAT:
    case RV2_VAL_FLOAT_RANGE:
        return a->fmin == b->fmin && a->fmax == b->fmax;
    case RV2_VAL_CIDR:
        return a->family == b->family && a->bits == b->bits && memcmp(&a->ip, &b->ip, sizeof(a->ip)) == 0;
    default:
        return strcmp(a->str, b->str) == 0;
    }
}
/******************************************************************************/
LOCAL gboolean rv2_node_equal(const ArkimeRulesV2Node_t *a, const ArkimeRulesV2Node_t *b)
{
    if (a->type != b->type || a->op != b->op || a->vtype != b->vtype || a->field != b->field ||
        a->i != b->i || a->f != b->f || a->func != b->func || a->andList != b->andList ||
        a->nvalues != b->nvalues || RV2_NKIDS(a) != RV2_NKIDS(b))
        return FALSE;
    if ((a->type == RV2_NODE_STR || a->type == RV2_NODE_CALL) && g_strcmp0(a->str, b->str) != 0)
        return FALSE;
    for (int v = 0; v < a->nvalues; v++) {
        if (!rv2_value_equal(&a->values[v], &b->values[v]))
            return FALSE;
    }
    for (int k = 0; k < RV2_NKIDS(a); k++) {
        if (!rv2_node_equal(RV2_KID(a, k), RV2_KID(b, k)))
            return FALSE;
    }
    return TRUE;
}
/******************************************************************************/
// Take a kid out of its parent, the parent can then be freed without it
LOCAL ArkimeRulesV2Node_t *rv2_detach(ArkimeRulesV2Node_t *node, int k)
{
    ArkimeRulesV2Node_t *kid = RV2_KID(node, k);
    g_ptr_array_index(node->kids, k) = NULL;
    return kid;
}
/******************************************************************************/
LOCAL ArkimeRulesV2Node_t *rv2_replace_bool(ArkimeRulesV2Node_t *node, gboolean value)
{
    ArkimeRulesV2Node_t *b = rv2_node_new(RV2_NODE_BOOL, node->start, node->end);
    b->vtype = RV2_VT_BOOL;
    b->i = value;
    arkime_rulesv2_free(node);
    return b;
}
/******************************************************************************/
LOCAL ArkimeRulesV2Node_t *rv2_replace_int(ArkimeRulesV2Node_t *node, int64_t value)
{
    ArkimeRulesV2Node_t *n = rv2_node_new(RV2_NODE_INT, node->start, node->end);
    n->vtype = RV2_VT_INT;
    n->i = value;
    arkime_rulesv2_free(node);
    return n;
}
/******************************************************************************/
LOCAL ArkimeRulesV2Node_t *rv2_replace_float(ArkimeRulesV2Node_t *node, double value)
{
    ArkimeRulesV2Node_t *n = rv2_node_new(RV2_NODE_FLOAT, node->start, node->end);
    n->vtype = RV2_VT_FLOAT;
    n->f = value;
    arkime_rulesv2_free(node);
    return n;
}
/******************************************************************************/
LOCAL ArkimeRulesV2Node_t *rv2_replace_str(ArkimeRulesV2Node_t *node, char *value)
{
    ArkimeRulesV2Node_t *n = rv2_node_new(RV2_NODE_STR, node->start, node->end);
    n->vtype = RV2_VT_STR;
    n->str = value;
    arkime_rulesv2_free(node);
    return n;
}
/******************************************************************************/
#define RV2_IS_NUM_CONST(n) ((n)->type == RV2_NODE_INT || (n)->type == RV2_NODE_FLOAT)
#define RV2_IS_CONST(n)     (RV2_IS_NUM_CONST(n) || (n)->type == RV2_NODE_STR)
#define RV2_NUM(n)          ((n)->type == RV2_NODE_INT ? (double)(n)->i : (n)->f)

// Same wrapping and division rules as the VM, returns FALSE when it can't fold
LOCAL gboolean rv2_fold_arith(RV2TokType op, const ArkimeRulesV2Node_t *a, const ArkimeRulesV2Node_t *b, gboolean isFloat, int64_t *i, double *f)
{
    if (isFloat) {
        const double x = RV2_NUM(a), y = RV2_NUM(b);
        switch (op) {
        case RV2_TOK_PLUS:
            *f = x + y;
            break;
        case RV2_TOK_MINUS:
            *f = x - y;
            break;
        case RV2_TOK_STAR:
            *f = x * y;
            break;
        default:
            *f = x / y;
            break;
        }
        return TRUE;
    }

    const int64_t x = a->i, y = b->i;
    switch (op) {
    case RV2_TOK_PLUS:
        *i = (int64_t)((uint64_t)x + (uint64_t)y);
        return TRUE;
    case RV2_TOK_MINUS:
        *i = (int64_t)((uint64_t)x - (uint64_t)y);
        return TRUE;
    case RV2_TOK_STAR:
        *i = (int64_t)((uint64_t)x * (uint64_t)y);
        return TRUE;
    default:
        // Division by zero is a missing value at runtime
        if (y == 0 || (x == INT64_MIN && y == -1))
            return FALSE;
        *i = op == RV2_TOK_SLASH ? x / y : x % y;
        return TRUE;
    }
}
/******************************************************************************/
LOCAL gboolean rv2_fold_match(ArkimeRulesV2Node_t *node, const ArkimeRulesV2Node_t *lhs)
{
    RV2Const_t k;
    gboolean result;

    switch (lhs->vtype) {
    case RV2_VT_INT:
        k.kind = RV2_K_INTSET;
        k.iset = rv2_intset_new(node->values, node->nvalues);
        result = rv2_intset_match(k.iset, lhs->i);
        break;
    case RV2_VT_FLOAT:
        k.kind = RV2_K_FLOATSET;
        k.fset = rv2_floatset_new(node->values, node->nvalues);
        result = rv2_floatset_match(k.fset, RV2_NUM(lhs));
        break;
    default:
        k.kind = RV2_K_STRSET;
        k.sset = rv2_strset_new(node->values, node->nvalues);
        result = rv2_strset_match(k.sset, lhs->str);
        break;
    }
    rv2_const_free(&k);
    return result;
}
/******************************************************************************/
LOCAL ArkimeRulesV2Node_t *rv2_fold(ArkimeRulesV2Node_t *node)
{
    const int nkids = RV2_NKIDS(node);
    ArkimeRulesV2Node_t *a = nkids > 0 ? RV2_KID(node, 0) : NULL;
    ArkimeRulesV2Node_t *b = nkids > 1 ? RV2_KID(node, 1) : NULL;
    int64_t i;
    double f;

    switch (node->type) {
    case RV2_NODE_NEG:
        if (a->type == RV2_NODE_INT)
            return rv2_replace_int(node, (int64_t)(0 - (uint64_t)a->i));
        if (a->type == RV2_NODE_FLOAT)
            return rv2_replace_float(node, -a->f);
        return node;

    case RV2_NODE_ARITH:
        if (!RV2_IS_NUM_CONST(a) || !RV2_IS_NUM_CONST(b))
            return node;
        if (!rv2_fold_arith(node->op, a, b, node->vtype == RV2_VT_FLOAT, &i, &f))
            return node;
        return node->vtype == RV2_VT_FLOAT ? rv2_replace_float(node, f) : rv2_replace_int(node, i);

    case RV2_NODE_CMP: {
        if (!RV2_IS_CONST(a) || !RV2_IS_CONST(b))
            return node;
        int cmp;
        if (a->type == RV2_NODE_STR)
            cmp = strcmp(a->str, b->str);
        else if (a->type == RV2_NODE_INT && b->type == RV2_NODE_INT)
            cmp = a->i < b->i ? -1 : a->i > b->i;
        else
            cmp = RV2_NUM(a) < RV2_NUM(b) ? -1 : RV2_NUM(a) > RV2_NUM(b);
        switch (node->op) {
        case RV2_TOK_LT:
            return rv2_replace_bool(node, cmp < 0);
        case RV2_TOK_LE:
            return rv2_replace_bool(node, cmp <= 0);
        case RV2_TOK_GT:
            return rv2_replace_bool(node, cmp > 0);
        case RV2_TOK_GE:
            return rv2_replace_bool(node, cmp >= 0);
        default:
            return rv2_replace_bool(node, cmp == 0);
        }
    }

    case RV2_NODE_MATCH:
        if (!RV2_IS_CONST(a))
            return node;
        return rv2_replace_bool(node, rv2_fold_match(node, a));

    case RV2_NODE_CALL: {
        const char *name = node->func->name;
        for (int k = 0; k < nkids; k++) {
            if (!RV2_IS_CONST(RV2_KID(node, k)))
                return node;
        }
        if (node->func->cb) {
            if (!(node->func->flags & RV2_FUNC_PURE) || node->vtype == RV2_VT_IP)
                return node;
            ArkimeRulesV2Value_t args[RV2_MAX_ARGS];
            for (int k = 0; k < nkids; k++) {
                const ArkimeRulesV2Node_t *arg = RV2_KID(node, k);
                if (node->func->args[k] == 'f')
                    args[k].f = RV2_NUM(arg);
                else if (arg->type == RV2_NODE_STR)
                    args[k].s = arg->str;
                else
                    args[k].i = arg->i;
            }
            ArkimeRulesV2Value_t result;
            if (!node->func->cb(NULL, args, nkids, &result, node->func->uw))
                return node;
            switch (node->vtype) {
            case RV2_VT_BOOL:
                return rv2_replace_bool(node, result.i != 0);
            case RV2_VT_INT:
                return rv2_replace_int(node, result.i);
            case RV2_VT_FLOAT:
                return rv2_replace_float(node, result.f);
            default:
                return rv2_replace_str(node, g_strdup(result.s));
            }
        }
        if (strcmp(name, "len") == 0)
            return rv2_replace_int(node, strlen(a->str));
        if (strcmp(name, "lower") == 0)
            return rv2_replace_str(node, g_ascii_strdown(a->str, -1));
        if (strcmp(name, "upper") == 0)
            return rv2_replace_str(node, g_ascii_strup(a->str, -1));
        if (strcmp(name, "abs") == 0) {
            if (node->vtype == RV2_VT_INT)
                return rv2_replace_int(node, a->i < 0 ? (int64_t)(0 - (uint64_t)a->i) : a->i);
            return rv2_replace_float(node, a->f < 0 ? -a->f : a->f);
        }
        if (strcmp(name, "min") == 0 || strcmp(name, "max") == 0) {
            const gboolean pickA = (name[1] == 'i') == (RV2_NUM(a) < RV2_NUM(b));
            const ArkimeRulesV2Node_t *pick = pickA ? a : b;
            if (node->vtype == RV2_VT_INT)
                return rv2_replace_int(node, pick->i);
            return rv2_replace_float(node, RV2_NUM(pick));
        }
        if (strcmp(name, "startsWith") == 0)
            return rv2_replace_bool(node, g_str_has_prefix(a->str, b->str));
        if (strcmp(name, "endsWith") == 0)
            return rv2_replace_bool(node, g_str_has_suffix(a->str, b->str));
        if (strcmp(name, "contains") == 0)
            return rv2_replace_bool(node, strstr(a->str, b->str) != NULL);
        if (strcmp(name, "icontains") == 0) {
            char *needle = g_ascii_strdown(b->str, -1);
            const gboolean found = rv2_icontains(a->str, needle, strlen(needle));
            g_free(needle);
            return rv2_replace_bool(node, found);
        }
        return node;
    }

    case RV2_NODE_NOT:
        if (a->type == RV2_NODE_BOOL)
            return rv2_replace_bool(node, !a->i);
        if (a->type == RV2_NODE_NOT) {
            ArkimeRulesV2Node_t *inner = rv2_detach(a, 0);
            arkime_rulesv2_free(node);
            return inner;
        }
        return node;

    case RV2_NODE_AND:
    case RV2_NODE_OR: {
        // identity is true for &&, false for ||, the other value decides the result
        const gboolean identity = node->type == RV2_NODE_AND;
        GPtrArray *kids = g_ptr_array_new_with_free_func((GDestroyNotify)arkime_rulesv2_free);

        for (int k = 0; k < nkids; k++) {
            ArkimeRulesV2Node_t *kid = rv2_detach(node, k);
            GPtrArray *flat = g_ptr_array_new();
            if (kid->type == node->type) {
                for (int g = 0; g < RV2_NKIDS(kid); g++)
                    g_ptr_array_add(flat, rv2_detach(kid, g));
                arkime_rulesv2_free(kid);
            } else {
                g_ptr_array_add(flat, kid);
            }

            for (guint g = 0; g < flat->len; g++) {
                ArkimeRulesV2Node_t *item = g_ptr_array_index(flat, g);
                gboolean drop = item->type == RV2_NODE_BOOL && item->i == identity;
                for (guint d = 0; !drop && d < kids->len; d++)
                    drop = rv2_node_equal(item, g_ptr_array_index(kids, d));
                if (drop)
                    arkime_rulesv2_free(item);
                else
                    g_ptr_array_add(kids, item);
            }
            g_ptr_array_free(flat, TRUE);
        }

        g_ptr_array_free(node->kids, TRUE);
        node->kids = kids;

        for (guint k = 0; k < kids->len; k++) {
            const ArkimeRulesV2Node_t *kid = g_ptr_array_index(kids, k);
            if (kid->type == RV2_NODE_BOOL)
                return rv2_replace_bool(node, !identity);
        }
        if (kids->len == 0)
            return rv2_replace_bool(node, identity);
        if (kids->len == 1) {
            ArkimeRulesV2Node_t *only = rv2_detach(node, 0);
            arkime_rulesv2_free(node);
            return only;
        }
        return node;
    }

    default:
        return node;
    }
}
/******************************************************************************/
// Values have no side effects, so matches on equal values can share one set
LOCAL gboolean rv2_mergeable_match(const ArkimeRulesV2Node_t *a, const ArkimeRulesV2Node_t *b)
{
    return a->type == RV2_NODE_MATCH && b->type == RV2_NODE_MATCH && !a->andList && !b->andList &&
           rv2_node_equal(RV2_KID(a, 0), RV2_KID(b, 0));
}
/******************************************************************************/
LOCAL void rv2_merge_values(ArkimeRulesV2Node_t *into, ArkimeRulesV2Node_t *from)
{
    into->values = g_renew(RV2Value_t, into->values, into->nvalues + from->nvalues);
    memcpy(into->values + into->nvalues, from->values, from->nvalues * sizeof(RV2Value_t));
    into->nvalues += from->nvalues;
    from->nvalues = 0;
    into->start = MIN(into->start, from->start);
    into->end = MAX(into->end, from->end);
}
/******************************************************************************/
LOCAL ArkimeRulesV2Node_t *rv2_optimize(ArkimeRulesV2Node_t *node);

/* In || merge f == a with f == b, and any(f, x) with any(f, y) into any(f, x || y).
 * In && merge !(f == a) with !(f == b), since that is !(f == a || f == b). */
LOCAL void rv2_merge(ArkimeRulesV2Node_t *node)
{
    GPtrArray *kids = node->kids;

    for (guint i = 0; i < kids->len; i++) {
        ArkimeRulesV2Node_t *a = g_ptr_array_index(kids, i);
        for (guint j = i + 1; j < kids->len; j++) {
            ArkimeRulesV2Node_t *b = g_ptr_array_index(kids, j);
            gboolean merged = FALSE;

            if (node->type == RV2_NODE_OR && rv2_mergeable_match(a, b)) {
                rv2_merge_values(a, b);
                merged = TRUE;
            } else if (node->type == RV2_NODE_AND && a->type == RV2_NODE_NOT && b->type == RV2_NODE_NOT &&
                       rv2_mergeable_match(RV2_KID(a, 0), RV2_KID(b, 0))) {
                rv2_merge_values(RV2_KID(a, 0), RV2_KID(b, 0));
                a->start = MIN(a->start, b->start);
                a->end = MAX(a->end, b->end);
                merged = TRUE;
            } else if (node->type == RV2_NODE_OR && rv2_is_call(a, "any") && rv2_is_call(b, "any") &&
                       a->i == b->i && RV2_KID(a, 0)->field == RV2_KID(b, 0)->field) {
                ArkimeRulesV2Node_t *or = rv2_node_new(RV2_NODE_OR, a->start, b->end);
                or->vtype = RV2_VT_BOOL;
                rv2_node_add_kid(or, rv2_detach(a, 1));
                rv2_node_add_kid(or, rv2_detach(b, 1));
                g_ptr_array_index(a->kids, 1) = rv2_optimize(or);
                a->start = MIN(a->start, b->start);
                a->end = MAX(a->end, b->end);
                merged = TRUE;
            }

            if (merged) {
                g_ptr_array_remove_index(kids, j);
                j--;
            }
        }
    }
}
/******************************************************************************/
/* Rough relative cost of evaluating a condition, so cheap tests short circuit
 * expensive ones. Internal string fields compute their value in the getCb,
 * the others just read the session. */
LOCAL int rv2_cost(const ArkimeRulesV2Node_t *node)
{
    int cost = 0;
    for (int k = 0; k < RV2_NKIDS(node); k++)
        cost += rv2_cost(RV2_KID(node, k));

    switch (node->type) {
    case RV2_NODE_BOOL:
    case RV2_NODE_INT:
    case RV2_NODE_FLOAT:
    case RV2_NODE_STR:
    case RV2_NODE_PLACEHOLDER:
        return 0;

    case RV2_NODE_FIELD:
        if (config.fields[node->field]->getCb && ARKIME_FIELD_TYPE_IS_STR(config.fields[node->field]->type))
            return 5;
        return node->multi ? 3 : 1;

    case RV2_NODE_EXISTS:
        return 1;

    case RV2_NODE_MATCH: {
        int worst = 0;
        for (int v = 0; v < node->nvalues; v++) {
            static const int valueCost[] = {
                [RV2_VAL_INT] = 1, [RV2_VAL_INT_RANGE] = 1, [RV2_VAL_FLOAT] = 1, [RV2_VAL_FLOAT_RANGE] = 1,
                [RV2_VAL_STR] = 2, [RV2_VAL_HEAD] = 2, [RV2_VAL_TAIL] = 2, [RV2_VAL_CONTAINS] = 3,
                [RV2_VAL_GLOB] = 4, [RV2_VAL_REGEX] = 10, [RV2_VAL_CIDR] = 2
            };
            worst = MAX(worst, valueCost[node->values[v].kind]);
        }
        return cost + worst;
    }

    case RV2_NODE_CALL: {
        const char *name = node->func->name;
        if (strcmp(name, "any") == 0 || strcmp(name, "all") == 0)
            return 4 + rv2_cost(RV2_KID(node, 0)) + 3 * rv2_cost(RV2_KID(node, 1));
        if (strcmp(name, "bpf") == 0)
            return 20;
        if (node->func->cb)
            return cost + 5;
        if (strcmp(name, "lower") == 0 || strcmp(name, "upper") == 0 || strcmp(name, "icontains") == 0)
            return cost + 3;
        return cost + 1;
    }

    default:
        return cost + 1;
    }
}
/******************************************************************************/
// Stable, so equal cost conditions keep the order they were written in
LOCAL void rv2_reorder(ArkimeRulesV2Node_t *node)
{
    const int n = RV2_NKIDS(node);
    int *costs = g_new(int, n);
    for (int k = 0; k < n; k++)
        costs[k] = rv2_cost(RV2_KID(node, k));

    for (int k = 1; k < n; k++) {
        gpointer kid = g_ptr_array_index(node->kids, k);
        const int cost = costs[k];
        int j = k - 1;
        for (; j >= 0 && costs[j] > cost; j--) {
            g_ptr_array_index(node->kids, j + 1) = g_ptr_array_index(node->kids, j);
            costs[j + 1] = costs[j];
        }
        g_ptr_array_index(node->kids, j + 1) = kid;
        costs[j + 1] = cost;
    }
    g_free(costs);
}
/******************************************************************************/
LOCAL ArkimeRulesV2Node_t *rv2_optimize(ArkimeRulesV2Node_t *node)
{
    for (int k = 0; k < RV2_NKIDS(node); k++)
        g_ptr_array_index(node->kids, k) = rv2_optimize(RV2_KID(node, k));

    node = rv2_fold(node);
    if (node->type != RV2_NODE_AND && node->type != RV2_NODE_OR)
        return node;

    rv2_merge(node);
    // Merging can leave duplicates or a single kid
    node = rv2_fold(node);
    if (node->type == RV2_NODE_AND || node->type == RV2_NODE_OR)
        rv2_reorder(node);
    return node;
}
/******************************************************************************/
/* Codegen, BPF style: every test has a true and false target, && || ! are just
 * targets, and every jump is forward. Labels are resolved per block at the end.
 * Value loads jump to the enclosing test's false target when the value is
 * missing, so under ! a missing value is true. */
/******************************************************************************/
typedef struct {
    RV2Parser_t     *p;
    GArray          *blocks;    // RV2Block_t
    GArray          *consts;    // RV2Const_t
    GArray          *insns;     // current block
    GArray          *spans;     // current block, one per insn
    GArray          *labels;    // current block, label to insn index
    GPtrArray       *cseNodes;  // values already in a register in the current leaf
    GArray          *cseRegs;
    int              nextReg;
    int              maxReg;
} RV2Gen_t;

/******************************************************************************/
LOCAL RV2Insn_t *rv2_emit(RV2Gen_t *gen, RV2Op op)
{
    RV2Insn_t insn;
    memset(&insn, 0, sizeof(insn));
    insn.op = op;
    insn.field = -1;
    g_array_append_val(gen->insns, insn);
    RV2Span_t span;
    memset(&span, 0, sizeof(span));
    g_array_append_val(gen->spans, span);
    return &g_array_index(gen->insns, RV2Insn_t, gen->insns->len - 1);
}
/******************************************************************************/
LOCAL int rv2_label_new(RV2Gen_t *gen)
{
    const int unplaced = -1;
    g_array_append_val(gen->labels, unplaced);
    return gen->labels->len - 1;
}
/******************************************************************************/
LOCAL void rv2_label_place(RV2Gen_t *gen, int label)
{
    g_array_index(gen->labels, int, label) = gen->insns->len;
}
/******************************************************************************/
LOCAL int rv2_reg_new(RV2Gen_t *gen, const ArkimeRulesV2Node_t *node)
{
    if (gen->nextReg >= RV2_MAX_REGS) {
        rv2_error(gen->p, node->start, "expression is too complex");
        return RV2_MAX_DEPTH;
    }
    const int reg = gen->nextReg++;
    if (gen->nextReg > gen->maxReg)
        gen->maxReg = gen->nextReg;
    return reg;
}
/******************************************************************************/
LOCAL int rv2_const_add(RV2Gen_t *gen, RV2Const_t *k)
{
    g_array_append_val(gen->consts, *k);
    return gen->consts->len - 1;
}
/******************************************************************************/
LOCAL int rv2_const_str(RV2Gen_t *gen, const char *str)
{
    RV2Const_t k = {.kind = RV2_K_STR};
    k.s.str = g_strdup(str);
    k.s.len = strlen(str);
    return rv2_const_add(gen, &k);
}
/******************************************************************************/
LOCAL int rv2_const_set(RV2Gen_t *gen, ArkimeRulesV2Node_t *match, int vtype)
{
    RV2Const_t k;
    switch (vtype) {
    case RV2_VT_INT:
        k.kind = RV2_K_INTSET;
        k.iset = rv2_intset_new(match->values, match->nvalues);
        break;
    case RV2_VT_FLOAT:
        k.kind = RV2_K_FLOATSET;
        k.fset = rv2_floatset_new(match->values, match->nvalues);
        break;
    case RV2_VT_STR:
        k.kind = RV2_K_STRSET;
        k.sset = rv2_strset_new(match->values, match->nvalues);
        break;
    default:
        k.kind = RV2_K_IPSET;
        k.ipset = rv2_ipset_new(match->values, match->nvalues);
        break;
    }
    return rv2_const_add(gen, &k);
}
/******************************************************************************/
LOCAL int rv2_gen_value(RV2Gen_t *gen, ArkimeRulesV2Node_t *node, int lmiss);

// Generate a value converted to vtype
LOCAL int rv2_gen_num(RV2Gen_t *gen, ArkimeRulesV2Node_t *node, int vtype, int lmiss)
{
    const int reg = rv2_gen_value(gen, node, lmiss);
    if (vtype != RV2_VT_FLOAT || node->vtype != RV2_VT_INT)
        return reg;

    const int freg = rv2_reg_new(gen, node);
    RV2Insn_t *insn = rv2_emit(gen, RV2_OP_I2F);
    insn->dst = freg;
    insn->a = reg;
    return freg;
}
/******************************************************************************/
LOCAL int rv2_gen_value_uncached(RV2Gen_t *gen, ArkimeRulesV2Node_t *node, int lmiss);

/* A leaf is straight line code that only exits early on a missing value, so a
 * value computed earlier in the leaf is always available later in it. */
LOCAL int rv2_gen_value(RV2Gen_t *gen, ArkimeRulesV2Node_t *node, int lmiss)
{
    if (node->type == RV2_NODE_PLACEHOLDER)
        return node->i;

    for (guint c = 0; c < gen->cseNodes->len; c++) {
        if (rv2_node_equal(node, g_ptr_array_index(gen->cseNodes, c)))
            return g_array_index(gen->cseRegs, int, c);
    }

    const int reg = rv2_gen_value_uncached(gen, node, lmiss);
    g_ptr_array_add(gen->cseNodes, node);
    g_array_append_val(gen->cseRegs, reg);
    return reg;
}
/******************************************************************************/
// Generate the args of a registered function call, returns its const
LOCAL int rv2_gen_call(RV2Gen_t *gen, ArkimeRulesV2Node_t *node, int lmiss)
{
    RV2Call_t *call = g_new0(RV2Call_t, 1);
    call->func = node->func;
    call->nargs = RV2_NKIDS(node);
    for (int a = 0; a < call->nargs; a++) {
        ArkimeRulesV2Node_t *arg = RV2_KID(node, a);
        call->regs[a] = node->func->args[a] == 'f' ? rv2_gen_num(gen, arg, RV2_VT_FLOAT, lmiss) : rv2_gen_value(gen, arg, lmiss);
    }

    RV2Const_t k = {.kind = RV2_K_CALL};
    k.call = call;
    return rv2_const_add(gen, &k);
}
/******************************************************************************/
LOCAL int rv2_gen_value_uncached(RV2Gen_t *gen, ArkimeRulesV2Node_t *node, int lmiss)
{
    RV2Insn_t *insn;
    RV2Const_t k;
    int reg, a, b;
    const gboolean isFloat = node->vtype == RV2_VT_FLOAT;

    switch (node->type) {
    case RV2_NODE_PLACEHOLDER:
        return node->i;

    case RV2_NODE_INT:
    case RV2_NODE_FLOAT:
        if (node->type == RV2_NODE_INT) {
            k.kind = RV2_K_INT;
            k.i = node->i;
        } else {
            k.kind = RV2_K_FLOAT;
            k.f = node->f;
        }
        a = rv2_const_add(gen, &k);
        reg = rv2_reg_new(gen, node);
        insn = rv2_emit(gen, node->type == RV2_NODE_INT ? RV2_OP_LOADK_INT : RV2_OP_LOADK_FLOAT);
        insn->dst = reg;
        insn->k = a;
        return reg;

    case RV2_NODE_STR:
        a = rv2_const_str(gen, node->str);
        reg = rv2_reg_new(gen, node);
        insn = rv2_emit(gen, RV2_OP_LOADK_STR);
        insn->dst = reg;
        insn->k = a;
        return reg;

    case RV2_NODE_FIELD: {
        static const RV2Op loads[] = {
            [RV2_VT_INT] = RV2_OP_LOAD_INT,
            [RV2_VT_FLOAT] = RV2_OP_LOAD_FLOAT,
            [RV2_VT_STR] = RV2_OP_LOAD_STR,
            [RV2_VT_IP] = RV2_OP_LOAD_IP
        };
        reg = rv2_reg_new(gen, node);
        insn = rv2_emit(gen, loads[node->vtype]);
        insn->dst = reg;
        insn->field = node->field;
        insn->jf = lmiss;
        return reg;
    }

    case RV2_NODE_NEG:
        a = rv2_gen_value(gen, RV2_KID(node, 0), lmiss);
        reg = rv2_reg_new(gen, node);
        insn = rv2_emit(gen, isFloat ? RV2_OP_NEG_F : RV2_OP_NEG_I);
        insn->dst = reg;
        insn->a = a;
        return reg;

    case RV2_NODE_ARITH: {
        RV2Op op;
        switch (node->op) {
        case RV2_TOK_PLUS:
            op = isFloat ? RV2_OP_ADD_F : RV2_OP_ADD_I;
            break;
        case RV2_TOK_MINUS:
            op = isFloat ? RV2_OP_SUB_F : RV2_OP_SUB_I;
            break;
        case RV2_TOK_STAR:
            op = isFloat ? RV2_OP_MUL_F : RV2_OP_MUL_I;
            break;
        case RV2_TOK_SLASH:
            op = isFloat ? RV2_OP_DIV_F : RV2_OP_DIV_I;
            break;
        default:
            op = RV2_OP_MOD_I;
            break;
        }
        a = rv2_gen_num(gen, RV2_KID(node, 0), node->vtype, lmiss);
        b = rv2_gen_num(gen, RV2_KID(node, 1), node->vtype, lmiss);
        reg = rv2_reg_new(gen, node);
        insn = rv2_emit(gen, op);
        insn->dst = reg;
        insn->a = a;
        insn->b = b;
        insn->jf = lmiss;
        return reg;
    }

    case RV2_NODE_CALL: {
        const char *name = node->func->name;
        if (node->func->cb) {
            static const RV2Op calls[] = {
                [RV2_VT_INT] = RV2_OP_CALL_I,
                [RV2_VT_FLOAT] = RV2_OP_CALL_F,
                [RV2_VT_STR] = RV2_OP_CALL_S,
                [RV2_VT_IP] = RV2_OP_CALL_IP
            };
            a = rv2_gen_call(gen, node, lmiss);
            reg = rv2_reg_new(gen, node);
            insn = rv2_emit(gen, calls[node->vtype]);
            insn->dst = reg;
            insn->k = a;
            insn->jf = lmiss;
            return reg;
        }
        if (strcmp(name, "count") == 0) {
            reg = rv2_reg_new(gen, node);
            insn = rv2_emit(gen, RV2_OP_COUNT);
            insn->dst = reg;
            insn->field = RV2_KID(node, 0)->field;
            return reg;
        }

        RV2Op op;
        if (strcmp(name, "len") == 0)
            op = RV2_OP_LEN;
        else if (strcmp(name, "lower") == 0)
            op = RV2_OP_LOWER;
        else if (strcmp(name, "upper") == 0)
            op = RV2_OP_UPPER;
        else if (strcmp(name, "abs") == 0)
            op = isFloat ? RV2_OP_ABS_F : RV2_OP_ABS_I;
        else if (strcmp(name, "min") == 0)
            op = isFloat ? RV2_OP_MIN_F : RV2_OP_MIN_I;
        else if (strcmp(name, "max") == 0)
            op = isFloat ? RV2_OP_MAX_F : RV2_OP_MAX_I;
        else
            break;

        a = rv2_gen_num(gen, RV2_KID(node, 0), node->vtype, lmiss);
        b = RV2_NKIDS(node) > 1 ? rv2_gen_num(gen, RV2_KID(node, 1), node->vtype, lmiss) : 0;
        reg = rv2_reg_new(gen, node);
        insn = rv2_emit(gen, op);
        insn->dst = reg;
        insn->a = a;
        insn->b = b;
        return reg;
    }

    default:
        break;
    }

    rv2_error(gen->p, node->start, "internal error, can't generate value");
    return 0;
}
/******************************************************************************/
LOCAL int rv2_gen_block(RV2Gen_t *gen, ArkimeRulesV2Node_t *cond, int depth);

LOCAL void rv2_gen_leaf(RV2Gen_t *gen, ArkimeRulesV2Node_t *node, int lt, int lf, gboolean neg)
{
    RV2Insn_t *insn = NULL;
    int a, b, k;

    switch (node->type) {
    case RV2_NODE_MATCH: {
        static const RV2Op fieldOps[] = {
            [RV2_VT_INT] = RV2_OP_MATCH_FIELD_INT,
            [RV2_VT_FLOAT] = RV2_OP_MATCH_FIELD_FLOAT,
            [RV2_VT_STR] = RV2_OP_MATCH_FIELD_STR,
            [RV2_VT_IP] = RV2_OP_MATCH_FIELD_IP
        };
        static const RV2Op regOps[] = {
            [RV2_VT_INT] = RV2_OP_MATCH_INT,
            [RV2_VT_FLOAT] = RV2_OP_MATCH_FLOAT,
            [RV2_VT_STR] = RV2_OP_MATCH_STR,
            [RV2_VT_IP] = RV2_OP_MATCH_IP
        };
        ArkimeRulesV2Node_t *lhs = RV2_KID(node, 0);
        k = rv2_const_set(gen, node, lhs->vtype);
        if (lhs->type == RV2_NODE_FIELD) {
            insn = rv2_emit(gen, fieldOps[lhs->vtype]);
            insn->field = lhs->field;
        } else {
            a = rv2_gen_value(gen, lhs, lf);
            insn = rv2_emit(gen, regOps[lhs->vtype]);
            insn->a = a;
        }
        insn->k = k;
        break;
    }

    case RV2_NODE_EXISTS:
        insn = rv2_emit(gen, RV2_OP_EXISTS);
        insn->field = node->field;
        break;

    case RV2_NODE_BOOL:
        insn = rv2_emit(gen, RV2_OP_BOOL);
        insn->b = node->i;
        break;

    case RV2_NODE_CMP: {
        ArkimeRulesV2Node_t *lhs = RV2_KID(node, 0);
        ArkimeRulesV2Node_t *rhs = RV2_KID(node, 1);
        int vtype = lhs->vtype;
        if (vtype == RV2_VT_INT && rhs->vtype == RV2_VT_FLOAT)
            vtype = RV2_VT_FLOAT;

        a = rv2_gen_num(gen, lhs, vtype, lf);
        b = rv2_gen_num(gen, rhs, vtype, lf);

        RV2Op op;
        if (vtype == RV2_VT_STR)
            op = RV2_OP_EQ_S;
        else if (vtype == RV2_VT_IP)
            op = RV2_OP_EQ_IP;
        else {
            const RV2Op base = vtype == RV2_VT_FLOAT ? RV2_OP_EQ_F : RV2_OP_EQ_I;
            switch (node->op) {
            case RV2_TOK_LT:
                op = base + 1;
                break;
            case RV2_TOK_LE:
                op = base + 2;
                break;
            case RV2_TOK_GT:
                op = base + 3;
                break;
            case RV2_TOK_GE:
                op = base + 4;
                break;
            default:
                op = base;
                break;
            }
        }
        insn = rv2_emit(gen, op);
        insn->a = a;
        insn->b = b;
        break;
    }

    case RV2_NODE_CALL: {
        const char *name = node->func->name;
        if (node->func->cb) {
            k = rv2_gen_call(gen, node, lf);
            insn = rv2_emit(gen, RV2_OP_CALL_B);
            insn->k = k;
        } else if (strcmp(name, "any") == 0 || strcmp(name, "all") == 0) {
            const int sub = rv2_gen_block(gen, RV2_KID(node, 1), node->i + 1);
            insn = rv2_emit(gen, name[1] == 'n' ? RV2_OP_ANY : RV2_OP_ALL);
            insn->field = RV2_KID(node, 0)->field;
            insn->a = node->i;
            insn->b = RV2_KID(node, 0)->vtype;
            insn->k = sub;
        } else if (strcmp(name, "has") == 0) {
            insn = rv2_emit(gen, RV2_OP_EXISTS);
            insn->field = RV2_KID(node, 0)->field;
        } else if (strcmp(name, "bpf") == 0) {
            RV2Const_t bk = {.kind = RV2_K_BPF};
            bk.bpf = g_new0(RV2Bpf_t, 1);
            bk.bpf->filter = g_strdup(RV2_KID(node, 0)->str);
            k = rv2_const_add(gen, &bk);
            insn = rv2_emit(gen, RV2_OP_BPF);
            insn->k = k;
        } else if (strcmp(name, "startsWith") == 0 || strcmp(name, "endsWith") == 0 ||
                   strcmp(name, "contains") == 0 || strcmp(name, "icontains") == 0) {
            const RV2Op op = name[0] == 's' ? RV2_OP_STARTS : name[0] == 'e' ? RV2_OP_ENDS : name[0] == 'c' ? RV2_OP_CONTAINS : RV2_OP_ICONTAINS;
            char *needle = op == RV2_OP_ICONTAINS ? g_ascii_strdown(RV2_KID(node, 1)->str, -1) : g_strdup(RV2_KID(node, 1)->str);
            k = rv2_const_str(gen, needle);
            g_free(needle);
            a = rv2_gen_value(gen, RV2_KID(node, 0), lf);
            insn = rv2_emit(gen, op);
            insn->a = a;
            insn->k = k;
        } else {
            const RV2Op op = strcmp(name, "isPrivate") == 0 ? RV2_OP_IS_PRIVATE :
                             strcmp(name, "isMulticast") == 0 ? RV2_OP_IS_MULTICAST :
                             strcmp(name, "isV4") == 0 ? RV2_OP_IS_V4 : RV2_OP_IS_V6;
            a = rv2_gen_value(gen, RV2_KID(node, 0), lf);
            insn = rv2_emit(gen, op);
            insn->a = a;
        }
        break;
    }

    default:
        rv2_error(gen->p, node->start, "internal error, can't generate condition");
        return;
    }

    insn->jt = lt;
    insn->jf = lf;

    // The test is always the last instruction of its leaf
    RV2Span_t *span = &g_array_index(gen->spans, RV2Span_t, gen->spans->len - 1);
    span->start = node->start;
    span->len = node->end - node->start;
    span->neg = neg;
    span->textNeg = node->textNeg;
}
/******************************************************************************/
LOCAL void rv2_gen_cond(RV2Gen_t *gen, ArkimeRulesV2Node_t *node, int lt, int lf, gboolean neg)
{
    const int nkids = RV2_NKIDS(node);

    switch (node->type) {
    case RV2_NODE_AND:
    case RV2_NODE_OR:
        for (int k = 0; k < nkids - 1; k++) {
            const int lnext = rv2_label_new(gen);
            if (node->type == RV2_NODE_AND)
                rv2_gen_cond(gen, RV2_KID(node, k), lnext, lf, neg);
            else
                rv2_gen_cond(gen, RV2_KID(node, k), lt, lnext, neg);
            rv2_label_place(gen, lnext);
        }
        rv2_gen_cond(gen, RV2_KID(node, nkids - 1), lt, lf, neg);
        break;

    case RV2_NODE_NOT:
        rv2_gen_cond(gen, RV2_KID(node, 0), lf, lt, !neg);
        break;

    default: {
        // Temp registers and the values in them only live within a leaf
        const int saved = gen->nextReg;
        g_ptr_array_set_size(gen->cseNodes, 0);
        g_array_set_size(gen->cseRegs, 0);
        rv2_gen_leaf(gen, node, lt, lf, neg);
        gen->nextReg = saved;
        g_ptr_array_set_size(gen->cseNodes, 0);
        g_array_set_size(gen->cseRegs, 0);
        break;
    }
    }
}
/******************************************************************************/
LOCAL int rv2_gen_block(RV2Gen_t *gen, ArkimeRulesV2Node_t *cond, int depth)
{
    const int idx = gen->blocks->len;
    RV2Block_t empty;
    memset(&empty, 0, sizeof(empty));
    g_array_append_val(gen->blocks, empty);

    GArray *savedInsns = gen->insns;
    GArray *savedSpans = gen->spans;
    GArray *savedLabels = gen->labels;
    gen->insns = g_array_new(FALSE, FALSE, sizeof(RV2Insn_t));
    gen->spans = g_array_new(FALSE, FALSE, sizeof(RV2Span_t));
    gen->labels = g_array_new(FALSE, FALSE, sizeof(int));

    const int lt = rv2_label_new(gen);
    const int lf = rv2_label_new(gen);
    rv2_gen_cond(gen, cond, lt, lf, FALSE);
    rv2_label_place(gen, lt);
    rv2_emit(gen, RV2_OP_RET_TRUE);
    rv2_label_place(gen, lf);
    rv2_emit(gen, RV2_OP_RET_FALSE);

    if (gen->insns->len >= RV2_JUMP_RET_FALSE)
        rv2_error(gen->p, cond->start, "expression is too large");

    for (guint i = 0; i < gen->insns->len; i++) {
        RV2Insn_t *insn = &g_array_index(gen->insns, RV2Insn_t, i);
        const int flags = rv2OpInfo[insn->op].flags;
        if (flags & RV2_F_TEST)
            insn->jt = g_array_index(gen->labels, int, insn->jt);
        if (flags & (RV2_F_TEST | RV2_F_MISS))
            insn->jf = g_array_index(gen->labels, int, insn->jf);
    }

    // Jumping to a RET returns directly, saving a dispatch
    for (guint i = 0; i < gen->insns->len; i++) {
        RV2Insn_t *insn = &g_array_index(gen->insns, RV2Insn_t, i);
        const int flags = rv2OpInfo[insn->op].flags;
        uint16_t *targets[2] = {(flags & RV2_F_TEST) ? &insn->jt : NULL, (flags & (RV2_F_TEST | RV2_F_MISS)) ? &insn->jf : NULL};
        for (int j = 0; j < 2; j++) {
            if (!targets[j])
                continue;
            const int op = g_array_index(gen->insns, RV2Insn_t, *targets[j]).op;
            if (op == RV2_OP_RET_TRUE)
                *targets[j] = RV2_JUMP_RET_TRUE;
            else if (op == RV2_OP_RET_FALSE)
                *targets[j] = RV2_JUMP_RET_FALSE;
        }
    }

    RV2Block_t *block = &g_array_index(gen->blocks, RV2Block_t, idx);
    block->depth = depth;
    block->ninsns = gen->insns->len;
    block->insns = (RV2Insn_t *)g_array_free(gen->insns, FALSE);
    block->spans = (RV2Span_t *)g_array_free(gen->spans, FALSE);
    g_array_free(gen->labels, TRUE);

    gen->insns = savedInsns;
    gen->spans = savedSpans;
    gen->labels = savedLabels;
    return idx;
}
/******************************************************************************/
/* Verifier, never trusts codegen. Checks every opcode, register, field, const
 * and block reference, that jumps only go forward and stay in the block, and
 * that every register is written with the right type on every path before it
 * is read. Blocks only run higher numbered blocks, so recursion is bounded. */
/******************************************************************************/
LOCAL void rv2_verify_merge(uint8_t *types, uint8_t *reached, int target, const uint8_t *state)
{
    if (target >= RV2_JUMP_RET_FALSE)
        return;
    uint8_t *ttypes = types + target * RV2_MAX_REGS;
    if (!reached[target]) {
        memcpy(ttypes, state, RV2_MAX_REGS);
        reached[target] = 1;
        return;
    }
    for (int r = 0; r < RV2_MAX_REGS; r++) {
        if (ttypes[r] != state[r])
            ttypes[r] = RV2_VT_NONE;
    }
}
/******************************************************************************/
LOCAL char *rv2_verify(ArkimeRulesV2Prog_t *prog)
{
    int *called = g_new0(int, prog->nblocks);
    char *error = NULL;

    if (prog->nregs > RV2_MAX_REGS)
        return g_strdup_printf("too many registers %d", prog->nregs);

    for (int b = 0; b < prog->nblocks && !error; b++) {
        RV2Block_t *block = &prog->blocks[b];
        const int n = block->ninsns;

        if ((b == 0) != (called[b] == 0) || called[b] > 1) {
            error = g_strdup_printf("block %d is called %d times", b, called[b]);
            break;
        }
        if (n == 0) {
            error = g_strdup_printf("block %d is empty", b);
            break;
        }

        uint8_t *types = g_malloc0(n * RV2_MAX_REGS);
        uint8_t *reached = g_malloc0(n);
        uint8_t out[RV2_MAX_REGS];

        reached[0] = 1;
        memcpy(types, block->entryTypes, block->depth);

        for (int pc = 0; pc < n && !error; pc++) {
            if (!reached[pc])
                continue;

            const RV2Insn_t *insn = &block->insns[pc];
            const uint8_t *in = types + pc * RV2_MAX_REGS;

#define RV2_VERIFY_FAIL(...) do { error = g_strdup_printf("block %d insn %d: ", b, pc); char *_m = g_strdup_printf(__VA_ARGS__); char *_e = g_strconcat(error, _m, NULL); g_free(error); g_free(_m); error = _e; goto done; } while (0)

            if (insn->op >= RV2_OP_NUM)
                RV2_VERIFY_FAIL("bad opcode %d", insn->op);
            const RV2OpInfo_t *info = &rv2OpInfo[insn->op];

            if (info->a && (insn->a >= prog->nregs || in[insn->a] != info->a))
                RV2_VERIFY_FAIL("%s reads register a %d before a %s is written", info->name, insn->a, rv2VtNames[info->a]);
            if (info->b && (insn->b >= prog->nregs || in[insn->b] != info->b))
                RV2_VERIFY_FAIL("%s reads register b %d before a %s is written", info->name, insn->b, rv2VtNames[info->b]);

            if (info->k && (insn->k >= prog->nconsts || prog->consts[insn->k].kind != info->k))
                RV2_VERIFY_FAIL("%s bad const %d", info->name, insn->k);

            if (info->flags & RV2_F_FIELD) {
                if (insn->field < 0 || insn->field >= ARKIME_FIELDS_MAX || !config.fields[insn->field])
                    RV2_VERIFY_FAIL("%s bad field %d", info->name, insn->field);
                uint8_t multi;
                const int fvtype = rv2_field_vtype(insn->field, &multi);
                int want = RV2_VT_NONE;
                switch (insn->op) {
                case RV2_OP_LOAD_INT:
                case RV2_OP_LOAD_FLOAT:
                case RV2_OP_LOAD_STR:
                case RV2_OP_LOAD_IP:
                    if (multi)
                        RV2_VERIFY_FAIL("%s of multi value field", info->name);
                    want = info->dst;
                    break;
                case RV2_OP_MATCH_FIELD_INT:
                    want = RV2_VT_INT;
                    break;
                case RV2_OP_MATCH_FIELD_FLOAT:
                    want = RV2_VT_FLOAT;
                    break;
                case RV2_OP_MATCH_FIELD_STR:
                    want = RV2_VT_STR;
                    break;
                case RV2_OP_MATCH_FIELD_IP:
                    want = RV2_VT_IP;
                    break;
                case RV2_OP_ANY:
                case RV2_OP_ALL:
                    want = insn->b;
                    break;
                default:
                    want = fvtype;
                    break;
                }
                if (fvtype != want)
                    RV2_VERIFY_FAIL("%s field %s is a %s not a %s", info->name, config.fields[insn->field]->expression, rv2VtNames[fvtype], rv2VtNames[want]);
            }

            if (info->flags & RV2_F_CALL) {
                const RV2Call_t *call = prog->consts[insn->k].call;
                const int ret = insn->op == RV2_OP_CALL_B ? RV2_VT_BOOL : info->dst;
                if (!call->func || !call->func->cb || call->func->ret != ret)
                    RV2_VERIFY_FAIL("%s function doesn't return a %s", info->name, rv2VtNames[ret]);
                if (call->nargs != (int)strlen(call->func->args) || call->nargs > RV2_MAX_ARGS)
                    RV2_VERIFY_FAIL("%s %s() has %d args", info->name, call->func->name, call->nargs);
                for (int a = 0; a < call->nargs; a++) {
                    const int want = rv2_type_char(call->func->args[a]);
                    if (call->regs[a] >= prog->nregs || in[call->regs[a]] != want)
                        RV2_VERIFY_FAIL("%s %s() arg %d register %d isn't a %s", info->name, call->func->name, a + 1, call->regs[a], rv2VtNames[want]);
                }
            }

            if (info->flags & RV2_F_SUB) {
                if (insn->a != block->depth || insn->a >= RV2_MAX_DEPTH)
                    RV2_VERIFY_FAIL("%s placeholder register %d at depth %d", info->name, insn->a, block->depth);
                if (insn->k <= b || insn->k >= prog->nblocks)
                    RV2_VERIFY_FAIL("%s bad block %d", info->name, insn->k);
                RV2Block_t *sub = &prog->blocks[insn->k];
                if (sub->depth != insn->a + 1)
                    RV2_VERIFY_FAIL("%s block %d depth %d", info->name, insn->k, sub->depth);
                memcpy(sub->entryTypes, in, insn->a);
                sub->entryTypes[insn->a] = insn->b;
                called[insn->k]++;
            }

            memcpy(out, in, RV2_MAX_REGS);
            if (info->dst) {
                if (insn->dst < RV2_MAX_DEPTH || insn->dst >= prog->nregs)
                    RV2_VERIFY_FAIL("%s bad dst register %d", info->name, insn->dst);
                out[insn->dst] = info->dst;
            }

            if (info->flags & RV2_F_RET)
                continue;

            const RV2Span_t *span = &block->spans[pc];
            if (span->start < 0 || span->len < 0 || span->start + span->len > prog->sourceLen)
                RV2_VERIFY_FAIL("%s source span %d+%d past %d", info->name, span->start, span->len, prog->sourceLen);

#define RV2_BAD_JUMP(t) ((t) < RV2_JUMP_RET_FALSE && ((t) <= pc || (t) >= n))
            if (info->flags & RV2_F_TEST) {
                if (RV2_BAD_JUMP(insn->jt) || RV2_BAD_JUMP(insn->jf))
                    RV2_VERIFY_FAIL("%s bad jump %d %d", info->name, insn->jt, insn->jf);
                rv2_verify_merge(types, reached, insn->jt, out);
                rv2_verify_merge(types, reached, insn->jf, out);
                continue;
            }

            if (pc + 1 >= n)
                RV2_VERIFY_FAIL("%s falls off the end", info->name);
            rv2_verify_merge(types, reached, pc + 1, out);

            if (info->flags & RV2_F_MISS) {
                if (RV2_BAD_JUMP(insn->jf))
                    RV2_VERIFY_FAIL("%s bad missing jump %d", info->name, insn->jf);
                rv2_verify_merge(types, reached, insn->jf, in);
            }
        }
done:
        g_free(types);
        g_free(reached);
    }

    for (int b = 1; b < prog->nblocks && !error; b++) {
        if (called[b] != 1)
            error = g_strdup_printf("block %d is called %d times", b, called[b]);
    }

    g_free(called);
    return error;
}
/******************************************************************************/
LOCAL void rv2_print_target(GString *out, int target)
{
    if (target == RV2_JUMP_RET_TRUE)
        g_string_append(out, " true");
    else if (target == RV2_JUMP_RET_FALSE)
        g_string_append(out, " false");
    else
        g_string_append_printf(out, " %d", target);
}
/******************************************************************************/
void arkime_rulesv2_disassemble(const ArkimeRulesV2Prog_t *prog, GString *out)
{
    for (int b = 0; b < prog->nblocks; b++) {
        const RV2Block_t *block = &prog->blocks[b];
        g_string_append_printf(out, "block %d depth %d:\n", b, block->depth);
        for (int pc = 0; pc < block->ninsns; pc++) {
            const RV2Insn_t *insn = &block->insns[pc];
            const RV2OpInfo_t *info = &rv2OpInfo[insn->op];
            g_string_append_printf(out, "  %3d: %-17s", pc, info->name);
            if (info->dst)
                g_string_append_printf(out, " r%d =", insn->dst);
            if (info->a)
                g_string_append_printf(out, " r%d", insn->a);
            if (info->b)
                g_string_append_printf(out, " r%d", insn->b);
            if (info->flags & RV2_F_FIELD)
                g_string_append_printf(out, " %s", config.fields[insn->field]->expression);
            if (info->flags & RV2_F_SUB)
                g_string_append_printf(out, " _r%d block %d", insn->a, insn->k);
            if (info->flags & RV2_F_CALL) {
                const RV2Call_t *call = prog->consts[insn->k].call;
                g_string_append_printf(out, " %s(", call->func->name);
                for (int a = 0; a < call->nargs; a++)
                    g_string_append_printf(out, "%sr%d", a ? ", " : "", call->regs[a]);
                g_string_append_c(out, ')');
            }
            if (info->k == RV2_K_INT)
                g_string_append_printf(out, " %" PRId64, prog->consts[insn->k].i);
            else if (info->k == RV2_K_FLOAT)
                g_string_append_printf(out, " %g", prog->consts[insn->k].f);
            else if (info->k == RV2_K_STR)
                g_string_append_printf(out, " \"%s\"", prog->consts[insn->k].s.str);
            else if (info->k == RV2_K_BPF)
                g_string_append_printf(out, " \"%s\"", prog->consts[insn->k].bpf->filter);
            else if (info->k && info->k != RV2_K_CALL)
                g_string_append_printf(out, " k%d", insn->k);
            if (info->flags & RV2_F_TEST) {
                g_string_append(out, " ?");
                rv2_print_target(out, insn->jt);
                g_string_append(out, " :");
                rv2_print_target(out, insn->jf);
            } else if (info->flags & RV2_F_MISS) {
                g_string_append(out, " missing");
                rv2_print_target(out, insn->jf);
            }
            g_string_append_c(out, '\n');
        }
    }
}
/******************************************************************************/
/* VM */
/******************************************************************************/
typedef struct {
    ArkimeSession_t           *session;
    ArkimePacket_t            *packet;
    const ArkimeRulesV2Prog_t *prog;
    GPtrArray                 *heap;        // lower()/upper() results that didn't fit in scratch
    GString                   *explain;     // set when explaining a match, only in block 0
    int                        scratchUsed;
    int                        triggerField; // fieldSet, the field just set and its value, -1 otherwise
    const void                *triggerValue;
    int                        ngetcb;
    int16_t                    getcbPos[8]; // internal field values, a getCb may compute its value
    void                      *getcbValue[8];
    RV2Reg_t                   regs[RV2_MAX_REGS];
    char                       scratch[2048];
} RV2Ctx_t;

#define RV2_INLINE static inline __attribute__((always_inline))

typedef gboolean (*RV2IntCb)(RV2Ctx_t *ctx, int64_t v, void *arg);
typedef gboolean (*RV2FloatCb)(RV2Ctx_t *ctx, double v, void *arg);
typedef gboolean (*RV2StrCb)(RV2Ctx_t *ctx, const char *v, void *arg);
typedef gboolean (*RV2IpCb)(RV2Ctx_t *ctx, const struct in6_addr *v, void *arg);

/******************************************************************************/
// Each internal field's getCb is called at most once per evaluation
RV2_INLINE void *rv2_getcb(RV2Ctx_t *ctx, int pos)
{
    for (int i = 0; i < ctx->ngetcb; i++) {
        if (ctx->getcbPos[i] == pos)
            return ctx->getcbValue[i];
    }
    void *value = config.fields[pos]->getCb(ctx->session, pos);
    if (ctx->ngetcb < (int)G_N_ELEMENTS(ctx->getcbPos)) {
        ctx->getcbPos[ctx->ngetcb] = pos;
        ctx->getcbValue[ctx->ngetcb++] = value;
    }
    return value;
}
/******************************************************************************/
LOCAL int64_t rv2_session_count(const ArkimeSession_t *session, int pos);

LOCAL int64_t rv2_count(RV2Ctx_t *ctx, int pos)
{
    const ArkimeSession_t *session = ctx->session;
    const ArkimeFieldInfo_t *info = config.fields[pos];

    if (pos >= config.minInternalField) {
        int64_t count = 0;
        if (info->getCb) {
            void *value = rv2_getcb(ctx, pos);
            switch (info->type) {
            case ARKIME_FIELD_TYPE_INT:
                count = 1;
                break;
            case ARKIME_FIELD_TYPE_STR:
            case ARKIME_FIELD_TYPE_IP:
                count = value != NULL;
                break;
            case ARKIME_FIELD_TYPE_STR_ARRAY:
                count = value ? ((GPtrArray *)value)->len : 0;
                break;
            case ARKIME_FIELD_TYPE_INT_ARRAY:
            case ARKIME_FIELD_TYPE_INT_ARRAY_UNIQUE:
                count = value ? ((GArray *)value)->len : 0;
                break;
            case ARKIME_FIELD_TYPE_STR_GHASH:
                count = value ? g_hash_table_size(value) : 0;
                break;
            default:
                break;
            }
        }
        // See RV2_TRIGGERED
        if (count == 0 && pos == ctx->triggerField)
            count = 1;
        return count;
    }

    return rv2_session_count(session, pos);
}
/******************************************************************************/
// Count for a non internal field
LOCAL int64_t rv2_session_count(const ArkimeSession_t *session, int pos)
{
    if (pos >= session->maxFields || !session->fields[pos])
        return 0;

    const ArkimeField_t *field = session->fields[pos];
    switch (config.fields[pos]->type) {
    case ARKIME_FIELD_TYPE_INT:
    case ARKIME_FIELD_TYPE_FLOAT:
    case ARKIME_FIELD_TYPE_STR:
    case ARKIME_FIELD_TYPE_IP:
        return 1;
    case ARKIME_FIELD_TYPE_INT_ARRAY:
    case ARKIME_FIELD_TYPE_INT_ARRAY_UNIQUE:
        return field->iarray->len;
    case ARKIME_FIELD_TYPE_FLOAT_ARRAY:
        return field->farray->len;
    case ARKIME_FIELD_TYPE_STR_ARRAY:
        return field->sarray->len;
    case ARKIME_FIELD_TYPE_INT_HASH:
        return HASH_COUNT(i_, *field->ihash);
    case ARKIME_FIELD_TYPE_STR_HASH:
        return HASH_COUNT(s_, *field->shash);
    case ARKIME_FIELD_TYPE_INT_GHASH:
    case ARKIME_FIELD_TYPE_STR_GHASH:
    case ARKIME_FIELD_TYPE_IP_GHASH:
    case ARKIME_FIELD_TYPE_FLOAT_GHASH:
        return g_hash_table_size(field->ghash);
    default:
        return 0;
    }
}
/******************************************************************************/
LOCAL gboolean rv2_exists(RV2Ctx_t *ctx, int pos)
{
    const ArkimeFieldInfo_t *info = config.fields[pos];
    return rv2_count(ctx, info->cntForPos ? info->cntForPos : pos) > 0;
}
/******************************************************************************/
/* Field iteration, calls cb for each value until it returns TRUE.
 * Returns -1 when there are no values, 1 when cb returned TRUE, else 0. */
RV2_INLINE int rv2_iter_int_values(RV2Ctx_t *ctx, int pos, RV2IntCb cb, void *arg)
{
    const ArkimeSession_t *session = ctx->session;
    const ArkimeFieldInfo_t *info = config.fields[pos];

    if (info->cntForPos)
        return cb(ctx, rv2_count(ctx, info->cntForPos), arg);

    if (pos >= config.minInternalField) {
        if (!info->getCb)
            return -1;
        void *value = rv2_getcb(ctx, pos);
        switch (info->type) {
        case ARKIME_FIELD_TYPE_INT:
            return cb(ctx, (long)value, arg);
        case ARKIME_FIELD_TYPE_INT_ARRAY:
        case ARKIME_FIELD_TYPE_INT_ARRAY_UNIQUE: {
            const GArray *iarray = value;
            if (!iarray || iarray->len == 0)
                return -1;
            for (guint i = 0; i < iarray->len; i++) {
                if (cb(ctx, g_array_index(iarray, uint32_t, i), arg))
                    return 1;
            }
            return 0;
        }
        default:
            return -1;
        }
    }

    if (pos >= session->maxFields || !session->fields[pos])
        return -1;

    const ArkimeField_t *field = session->fields[pos];
    switch (info->type) {
    case ARKIME_FIELD_TYPE_INT:
        return cb(ctx, field->i, arg);
    case ARKIME_FIELD_TYPE_INT_ARRAY:
    case ARKIME_FIELD_TYPE_INT_ARRAY_UNIQUE:
        for (guint i = 0; i < field->iarray->len; i++) {
            if (cb(ctx, g_array_index(field->iarray, uint32_t, i), arg))
                return 1;
        }
        return 0;
    case ARKIME_FIELD_TYPE_INT_HASH: {
        const ArkimeInt_t *hint;
        HASH_FORALL2(i_, *field->ihash, hint) {
            if (cb(ctx, hint->i_hash, arg))
                return 1;
        }
        return 0;
    }
    case ARKIME_FIELD_TYPE_INT_GHASH: {
        GHashTableIter iter;
        gpointer ikey;
        g_hash_table_iter_init(&iter, field->ghash);
        while (g_hash_table_iter_next(&iter, &ikey, NULL)) {
            if (cb(ctx, (long)ikey, arg))
                return 1;
        }
        return 0;
    }
    default:
        return -1;
    }
}
/******************************************************************************/
RV2_INLINE int rv2_iter_float(RV2Ctx_t *ctx, int pos, RV2FloatCb cb, void *arg)
{
    const ArkimeSession_t *session = ctx->session;
    const ArkimeFieldInfo_t *info = config.fields[pos];

    if (pos >= config.minInternalField || pos >= session->maxFields || !session->fields[pos])
        return -1;

    const ArkimeField_t *field = session->fields[pos];
    switch (info->type) {
    case ARKIME_FIELD_TYPE_FLOAT:
        return cb(ctx, field->f, arg);
    case ARKIME_FIELD_TYPE_FLOAT_ARRAY:
        for (guint i = 0; i < field->farray->len; i++) {
            if (cb(ctx, g_array_index(field->farray, float, i), arg))
                return 1;
        }
        return 0;
    case ARKIME_FIELD_TYPE_FLOAT_GHASH: {
        GHashTableIter iter;
        gpointer fkey;
        g_hash_table_iter_init(&iter, field->ghash);
        while (g_hash_table_iter_next(&iter, &fkey, NULL)) {
            if (cb(ctx, POINTER_TO_FLOAT(fkey), arg))
                return 1;
        }
        return 0;
    }
    default:
        return -1;
    }
}
/******************************************************************************/
RV2_INLINE int rv2_iter_str_values(RV2Ctx_t *ctx, int pos, RV2StrCb cb, void *arg)
{
    const ArkimeSession_t *session = ctx->session;
    const ArkimeFieldInfo_t *info = config.fields[pos];
    GHashTableIter iter;
    gpointer ikey;

    if (pos >= config.minInternalField) {
        if (!info->getCb)
            return -1;
        void *value = rv2_getcb(ctx, pos);
        if (!value)
            return -1;
        switch (info->type) {
        case ARKIME_FIELD_TYPE_STR:
            return cb(ctx, value, arg);
        case ARKIME_FIELD_TYPE_STR_ARRAY: {
            const GPtrArray *sarray = value;
            if (sarray->len == 0)
                return -1;
            for (guint i = 0; i < sarray->len; i++) {
                if (cb(ctx, g_ptr_array_index(sarray, i), arg))
                    return 1;
            }
            return 0;
        }
        case ARKIME_FIELD_TYPE_STR_GHASH:
            if (g_hash_table_size(value) == 0)
                return -1;
            g_hash_table_iter_init(&iter, value);
            while (g_hash_table_iter_next(&iter, &ikey, NULL)) {
                if (cb(ctx, ikey, arg))
                    return 1;
            }
            return 0;
        default:
            return -1;
        }
    }

    if (pos >= session->maxFields || !session->fields[pos])
        return -1;

    const ArkimeField_t *field = session->fields[pos];
    switch (info->type) {
    case ARKIME_FIELD_TYPE_STR:
        return cb(ctx, field->str, arg);
    case ARKIME_FIELD_TYPE_STR_ARRAY:
        for (guint i = 0; i < field->sarray->len; i++) {
            if (cb(ctx, g_ptr_array_index(field->sarray, i), arg))
                return 1;
        }
        return 0;
    case ARKIME_FIELD_TYPE_STR_HASH: {
        const ArkimeString_t *hstring;
        HASH_FORALL2(s_, *field->shash, hstring) {
            if (cb(ctx, hstring->str, arg))
                return 1;
        }
        return 0;
    }
    case ARKIME_FIELD_TYPE_STR_GHASH:
        g_hash_table_iter_init(&iter, field->ghash);
        while (g_hash_table_iter_next(&iter, &ikey, NULL)) {
            if (cb(ctx, ikey, arg))
                return 1;
        }
        return 0;
    default:
        return -1;
    }
}
/******************************************************************************/
RV2_INLINE int rv2_iter_ip_values(RV2Ctx_t *ctx, int pos, RV2IpCb cb, void *arg)
{
    const ArkimeSession_t *session = ctx->session;
    const ArkimeFieldInfo_t *info = config.fields[pos];

    if (pos >= config.minInternalField) {
        if (!info->getCb || info->type != ARKIME_FIELD_TYPE_IP)
            return -1;
        void *value = rv2_getcb(ctx, pos);
        if (!value)
            return -1;
        return cb(ctx, value, arg);
    }

    if (pos >= session->maxFields || !session->fields[pos])
        return -1;

    const ArkimeField_t *field = session->fields[pos];
    switch (info->type) {
    case ARKIME_FIELD_TYPE_IP:
        return cb(ctx, field->ip, arg);
    case ARKIME_FIELD_TYPE_IP_GHASH: {
        GHashTableIter iter;
        gpointer ikey;
        g_hash_table_iter_init(&iter, field->ghash);
        while (g_hash_table_iter_next(&iter, &ikey, NULL)) {
            if (cb(ctx, ikey, arg))
                return 1;
        }
        return 0;
    }
    default:
        return -1;
    }
}
/******************************************************************************/
/* An internal field's value may not be readable until its parser finishes, so
 * for fieldSet the value that triggered is also one of its values. Non internal
 * fields already have it. */
#define RV2_TRIGGERED(ctx, pos) (G_UNLIKELY((pos) == (ctx)->triggerField) && (pos) >= config.minInternalField)

RV2_INLINE int rv2_iter_int(RV2Ctx_t *ctx, int pos, RV2IntCb cb, void *arg)
{
    const int r = rv2_iter_int_values(ctx, pos, cb, arg);
    if (r != 1 && RV2_TRIGGERED(ctx, pos))
        return cb(ctx, (long)ctx->triggerValue, arg);
    return r;
}
RV2_INLINE int rv2_iter_str(RV2Ctx_t *ctx, int pos, RV2StrCb cb, void *arg)
{
    const int r = rv2_iter_str_values(ctx, pos, cb, arg);
    if (r != 1 && RV2_TRIGGERED(ctx, pos))
        return cb(ctx, (const char *)ctx->triggerValue, arg);
    return r;
}
RV2_INLINE int rv2_iter_ip(RV2Ctx_t *ctx, int pos, RV2IpCb cb, void *arg)
{
    const int r = rv2_iter_ip_values(ctx, pos, cb, arg);
    if (r != 1 && RV2_TRIGGERED(ctx, pos))
        return cb(ctx, (const struct in6_addr *)ctx->triggerValue, arg);
    return r;
}
/******************************************************************************/
LOCAL gboolean rv2_cb_store_int(RV2Ctx_t *UNUSED(ctx), int64_t v, void *arg)
{
    ((RV2Reg_t *)arg)->i = v;
    return TRUE;
}
LOCAL gboolean rv2_cb_store_float(RV2Ctx_t *UNUSED(ctx), double v, void *arg)
{
    ((RV2Reg_t *)arg)->f = v;
    return TRUE;
}
LOCAL gboolean rv2_cb_store_str(RV2Ctx_t *UNUSED(ctx), const char *v, void *arg)
{
    ((RV2Reg_t *)arg)->s = v;
    return TRUE;
}
LOCAL gboolean rv2_cb_store_ip(RV2Ctx_t *UNUSED(ctx), const struct in6_addr *v, void *arg)
{
    ((RV2Reg_t *)arg)->ip = v;
    return TRUE;
}
LOCAL gboolean rv2_cb_intset(RV2Ctx_t *UNUSED(ctx), int64_t v, void *arg)
{
    return rv2_intset_match(arg, v);
}
LOCAL gboolean rv2_cb_floatset(RV2Ctx_t *UNUSED(ctx), double v, void *arg)
{
    return rv2_floatset_match(arg, v);
}
// Exact, head, tail and contains only, smaller than rv2_strset_match
LOCAL __attribute__((noinline)) gboolean rv2_strset_match_plain(const RV2StrSet_t *set, const char *str)
{
    for (int i = 0; i < set->nexact; i++) {
        if (set->exact[i].str[0] == str[0] && strcmp(set->exact[i].str, str) == 0)
            return TRUE;
    }
    const int len = strlen(str);
    for (int i = 0; i < set->ntail; i++) {
        if (len >= set->tail[i].len && memcmp(str + len - set->tail[i].len, set->tail[i].str, set->tail[i].len) == 0)
            return TRUE;
    }
    for (int i = 0; i < set->nhead; i++) {
        if (len >= set->head[i].len && memcmp(str, set->head[i].str, set->head[i].len) == 0)
            return TRUE;
    }
    for (int i = 0; i < set->ncontains; i++) {
        if (arkime_memstr(str, len, set->contains[i].str, set->contains[i].len))
            return TRUE;
    }
    return FALSE;
}
/******************************************************************************/
LOCAL gboolean rv2_cb_strset(RV2Ctx_t *UNUSED(ctx), const char *v, void *arg)
{
    const RV2StrSet_t *set = arg;
    if (set->fewExact) {
        for (int i = 0; i < set->nexact; i++) {
            if (set->exact[i].str[0] == v[0] && strcmp(set->exact[i].str, v) == 0)
                return TRUE;
        }
        return FALSE;
    }
    if (set->hashOnly)
        return rv2_strhash_contains(set, v);
    if (set->tailOnly) {
        const int len = strlen(v);
        for (int i = 0; i < set->ntail; i++) {
            if (len >= set->tail[i].len && memcmp(v + len - set->tail[i].len, set->tail[i].str, set->tail[i].len) == 0)
                return TRUE;
        }
        return FALSE;
    }
    if (set->noPatterns)
        return rv2_strset_match_plain(set, v);
    return rv2_strset_match(set, v);
}
LOCAL gboolean rv2_cb_ipset(RV2Ctx_t *UNUSED(ctx), const struct in6_addr *v, void *arg)
{
    return rv2_ipset_match(arg, v);
}
/******************************************************************************/
typedef struct {
    int              block;
    int              reg;
    gboolean         negate;
} RV2Sub_t;

LOCAL gboolean rv2_exec(RV2Ctx_t *ctx, int b);

// Run the sub block, anything lower()/upper() allocated is released after
RV2_INLINE gboolean rv2_run_sub(RV2Ctx_t *ctx, const RV2Sub_t *sub)
{
    const int scratchUsed = ctx->scratchUsed;
    const guint heapLen = ctx->heap ? ctx->heap->len : 0;
    GString *explain = ctx->explain;
    ctx->explain = NULL;
    const gboolean result = rv2_exec(ctx, sub->block);
    ctx->explain = explain;
    ctx->scratchUsed = scratchUsed;
    if (ctx->heap)
        g_ptr_array_set_size(ctx->heap, heapLen);
    return result != sub->negate;
}
LOCAL gboolean rv2_cb_sub_int(RV2Ctx_t *ctx, int64_t v, void *arg)
{
    ctx->regs[((RV2Sub_t *)arg)->reg].i = v;
    return rv2_run_sub(ctx, arg);
}
LOCAL gboolean rv2_cb_sub_float(RV2Ctx_t *ctx, double v, void *arg)
{
    ctx->regs[((RV2Sub_t *)arg)->reg].f = v;
    return rv2_run_sub(ctx, arg);
}
LOCAL gboolean rv2_cb_sub_str(RV2Ctx_t *ctx, const char *v, void *arg)
{
    ctx->regs[((RV2Sub_t *)arg)->reg].s = v;
    return rv2_run_sub(ctx, arg);
}
LOCAL gboolean rv2_cb_sub_ip(RV2Ctx_t *ctx, const struct in6_addr *v, void *arg)
{
    ctx->regs[((RV2Sub_t *)arg)->reg].ip = v;
    return rv2_run_sub(ctx, arg);
}
/******************************************************************************/
// all() is any() of the negated predicate, and false when there are no values
LOCAL int rv2_iter_sub(RV2Ctx_t *ctx, const RV2Insn_t *insn, gboolean negate)
{
    RV2Sub_t sub = {.block = insn->k, .reg = insn->a, .negate = negate};
    switch (insn->b) {
    case RV2_VT_INT:
        return rv2_iter_int(ctx, insn->field, rv2_cb_sub_int, &sub);
    case RV2_VT_FLOAT:
        return rv2_iter_float(ctx, insn->field, rv2_cb_sub_float, &sub);
    case RV2_VT_STR:
        return rv2_iter_str(ctx, insn->field, rv2_cb_sub_str, &sub);
    case RV2_VT_IP:
        return rv2_iter_ip(ctx, insn->field, rv2_cb_sub_ip, &sub);
    default:
        return -1;
    }
}
/******************************************************************************/
LOCAL const char *rv2_strcase(RV2Ctx_t *ctx, const char *str, gboolean lower)
{
    const int len = strlen(str);
    char *out;

    if (ctx->scratchUsed + len + 1 <= (int)sizeof(ctx->scratch)) {
        out = ctx->scratch + ctx->scratchUsed;
        ctx->scratchUsed += len + 1;
    } else {
        out = g_malloc(len + 1);
        if (!ctx->heap)
            ctx->heap = g_ptr_array_new_with_free_func(g_free);
        g_ptr_array_add(ctx->heap, out);
    }

    for (int i = 0; i < len; i++)
        out[i] = lower ? g_ascii_tolower(str[i]) : g_ascii_toupper(str[i]);
    out[len] = 0;
    return out;
}
/******************************************************************************/
LOCAL gboolean rv2_icontains(const char *str, const char *needle, int needleLen)
{
    const int len = strlen(str);
    for (int i = 0; i + needleLen <= len; i++) {
        if (g_ascii_strncasecmp(str + i, needle, needleLen) == 0)
            return TRUE;
    }
    return FALSE;
}
/******************************************************************************/
// RFC 1918, CGNAT, loopback and link local, v6 unique local, link local and loopback
LOCAL gboolean rv2_ip_private(const struct in6_addr *ip)
{
    if (IN6_IS_ADDR_V4MAPPED(ip)) {
        const uint8_t *a = ip->s6_addr + 12;
        return a[0] == 10 || a[0] == 127 ||
               (a[0] == 172 && (a[1] & 0xf0) == 16) ||
               (a[0] == 192 && a[1] == 168) ||
               (a[0] == 169 && a[1] == 254) ||
               (a[0] == 100 && (a[1] & 0xc0) == 64);
    }
    return (ip->s6_addr[0] & 0xfe) == 0xfc ||
           (ip->s6_addr[0] == 0xfe && (ip->s6_addr[1] & 0xc0) == 0x80) ||
           IN6_IS_ADDR_LOOPBACK(ip);
}
/******************************************************************************/
LOCAL gboolean rv2_ip_multicast(const struct in6_addr *ip)
{
    if (IN6_IS_ADDR_V4MAPPED(ip))
        return (ip->s6_addr[12] & 0xf0) == 0xe0;
    return ip->s6_addr[0] == 0xff;
}
/******************************************************************************/
LOCAL gboolean rv2_bpf_match(const RV2Ctx_t *ctx, RV2Bpf_t *bpf)
{
    if (!ctx->packet)
        return FALSE;
    const int dltIndex = arkime_packet_dlt_index(ctx->packet);
    if (dltIndex < 0)
        return FALSE;
    const struct bpf_program *bpfp = ARKIME_THREAD_ATOMIC_LOAD(bpf->bpfp[dltIndex]);
    return bpfp && bpfp->bf_len && bpf_filter(bpfp->bf_insns, ctx->packet->pkt, ctx->packet->pktlen, ctx->packet->pktlen);
}
/******************************************************************************/
LOCAL gboolean rv2_call(const RV2Ctx_t *ctx, const RV2Call_t *call, RV2Reg_t *result)
{
    ArkimeRulesV2Value_t args[RV2_MAX_ARGS];
    for (int a = 0; a < call->nargs; a++)
        args[a] = ctx->regs[call->regs[a]];
    return call->func->cb(ctx->session, args, call->nargs, result, call->func->uw);
}
/******************************************************************************/
/* Explain, used to log what matched for log: true rules. Every test that went
 * the way that leads to a match records its source and the value involved. A
 * test that passed on a path that later failed (a && b || c with b false) is
 * also listed. */
/******************************************************************************/
typedef struct {
    void            *set;
    RV2Reg_t         reg;
} RV2Find_t;

LOCAL gboolean rv2_cb_find_int(RV2Ctx_t *UNUSED(ctx), int64_t v, void *arg)
{
    RV2Find_t *find = arg;
    find->reg.i = v;
    return rv2_intset_match(find->set, v);
}
LOCAL gboolean rv2_cb_find_float(RV2Ctx_t *UNUSED(ctx), double v, void *arg)
{
    RV2Find_t *find = arg;
    find->reg.f = v;
    return rv2_floatset_match(find->set, v);
}
LOCAL gboolean rv2_cb_find_str(RV2Ctx_t *UNUSED(ctx), const char *v, void *arg)
{
    RV2Find_t *find = arg;
    find->reg.s = v;
    return rv2_strset_match(find->set, v);
}
LOCAL gboolean rv2_cb_find_ip(RV2Ctx_t *UNUSED(ctx), const struct in6_addr *v, void *arg)
{
    RV2Find_t *find = arg;
    find->reg.ip = v;
    return rv2_ipset_match(find->set, v);
}
/******************************************************************************/
LOCAL void rv2_print_reg(GString *out, const RV2Reg_t *reg, int vtype)
{
    char ipstr[INET6_ADDRSTRLEN];

    switch (vtype) {
    case RV2_VT_INT:
        g_string_append_printf(out, "%" PRId64, reg->i);
        break;
    case RV2_VT_FLOAT:
        g_string_append_printf(out, "%g", reg->f);
        break;
    case RV2_VT_STR:
        if (strlen(reg->s) > 100)
            g_string_append_printf(out, "%.100s...", reg->s);
        else
            g_string_append(out, reg->s);
        break;
    case RV2_VT_IP:
        if (IN6_IS_ADDR_V4MAPPED(reg->ip))
            g_string_append(out, inet_ntop(AF_INET, reg->ip->s6_addr + 12, ipstr, sizeof(ipstr)));
        else
            g_string_append(out, inet_ntop(AF_INET6, reg->ip, ipstr, sizeof(ipstr)));
        break;
    }
}
/******************************************************************************/
#define RV2_EXPLAIN_MAX_TEXT 200

LOCAL void rv2_explain(RV2Ctx_t *ctx, const RV2Insn_t *insn, gboolean result)
{
    // Only runs in block 0
    const RV2Block_t *block = &ctx->prog->blocks[0];
    const RV2Span_t *span = &block->spans[insn - block->insns];
    if (result == span->neg || span->len == 0)
        return;

    GString *out = ctx->explain;
    const char *src = ctx->prog->source + span->start;
    const int len = MIN(span->len, RV2_EXPLAIN_MAX_TEXT);
    const char *more = span->len > RV2_EXPLAIN_MAX_TEXT ? "..." : "";
    const RV2OpInfo_t *info = &rv2OpInfo[insn->op];
    const RV2Const_t *k = &ctx->prog->consts[insn->k];

    // != text matched by its lowered == under !, or !(a != b) folded to a == b
    if (span->neg != span->textNeg)
        g_string_append_printf(out, "!(%.*s%s)", len, src, more);
    else
        g_string_append_printf(out, "%.*s%s", len, src, more);
    if (span->neg) {
        g_string_append(out, ", ");
        return;
    }

    RV2Find_t find = {.set = NULL};
    int found = RV2_VT_NONE;
    switch (insn->op) {
    case RV2_OP_MATCH_FIELD_INT:
        find.set = k->iset;
        found = rv2_iter_int(ctx, insn->field, rv2_cb_find_int, &find) == 1 ? RV2_VT_INT : 0;
        break;
    case RV2_OP_MATCH_FIELD_FLOAT:
        find.set = k->fset;
        found = rv2_iter_float(ctx, insn->field, rv2_cb_find_float, &find) == 1 ? RV2_VT_FLOAT : 0;
        break;
    case RV2_OP_MATCH_FIELD_STR:
        find.set = k->sset;
        found = rv2_iter_str(ctx, insn->field, rv2_cb_find_str, &find) == 1 ? RV2_VT_STR : 0;
        break;
    case RV2_OP_MATCH_FIELD_IP:
        find.set = k->ipset;
        found = rv2_iter_ip(ctx, insn->field, rv2_cb_find_ip, &find) == 1 ? RV2_VT_IP : 0;
        break;
    case RV2_OP_ANY:
        // Iteration stopped on the value that matched
        find.reg = ctx->regs[insn->a];
        found = insn->b;
        break;
    default:
        if (info->a) {
            find.reg = ctx->regs[insn->a];
            found = info->a;
        }
        break;
    }

    if (found) {
        g_string_append(out, " (");
        rv2_print_reg(out, &find.reg, found);
        if (info->b && !(info->flags & RV2_F_SUB)) {
            g_string_append(out, ", ");
            rv2_print_reg(out, &ctx->regs[insn->b], info->b);
        }
        g_string_append_c(out, ')');
    }
    g_string_append(out, ", ");
}
/******************************************************************************/
#define RV2_OP_LABEL(name, flags, dst, a, b, k) &&op_##name,

LOCAL gboolean rv2_exec(RV2Ctx_t *ctx, int b)
{
    static const void *const labels[] = {
        RV2_OPS(RV2_OP_LABEL)
    };

    const RV2Insn_t *const insns = ctx->prog->blocks[b].insns;
    const RV2Const_t *const consts = ctx->prog->consts;
    RV2Reg_t *const regs = ctx->regs;
    const RV2Insn_t *insn = insns;

#define RV2_DISPATCH()  goto *labels[insn->op]
#define RV2_NEXT()      do { insn++; RV2_DISPATCH(); } while (0)
#define RV2_JUMP(_t)    do { \
        const uint16_t _target = (_t); \
        if (_target >= RV2_JUMP_RET_FALSE) \
            return _target == RV2_JUMP_RET_TRUE; \
        insn = insns + _target; \
        RV2_DISPATCH(); \
    } while (0)
#define RV2_BRANCH(_c)  do { \
        const gboolean _r = (_c); \
        if (G_UNLIKELY(ctx->explain)) \
            rv2_explain(ctx, insn, _r); \
        RV2_JUMP(_r ? insn->jt : insn->jf); \
    } while (0)
#define RV2_MISSING()   RV2_JUMP(insn->jf)
#define RA              regs[insn->a]
#define RB              regs[insn->b]
#define RD              regs[insn->dst]
#define K               consts[insn->k]

    RV2_DISPATCH();

op_RET_TRUE:
    return TRUE;
op_RET_FALSE:
    return FALSE;

op_LOADK_INT:
    RD.i = K.i;
    RV2_NEXT();
op_LOADK_FLOAT:
    RD.f = K.f;
    RV2_NEXT();
op_LOADK_STR:
    RD.s = K.s.str;
    RV2_NEXT();

op_LOAD_INT:
    if (rv2_iter_int(ctx, insn->field, rv2_cb_store_int, &RD) != 1)
        RV2_MISSING();
    RV2_NEXT();
op_LOAD_FLOAT:
    if (rv2_iter_float(ctx, insn->field, rv2_cb_store_float, &RD) != 1)
        RV2_MISSING();
    RV2_NEXT();
op_LOAD_STR:
    if (rv2_iter_str(ctx, insn->field, rv2_cb_store_str, &RD) != 1)
        RV2_MISSING();
    RV2_NEXT();
op_LOAD_IP:
    if (rv2_iter_ip(ctx, insn->field, rv2_cb_store_ip, &RD) != 1)
        RV2_MISSING();
    RV2_NEXT();

op_COUNT:
    RD.i = rv2_count(ctx, insn->field);
    RV2_NEXT();
op_I2F:
    RD.f = RA.i;
    RV2_NEXT();

    // Integer math wraps instead of overflowing
op_ADD_I:
    RD.i = (int64_t)((uint64_t)RA.i + (uint64_t)RB.i);
    RV2_NEXT();
op_SUB_I:
    RD.i = (int64_t)((uint64_t)RA.i - (uint64_t)RB.i);
    RV2_NEXT();
op_MUL_I:
    RD.i = (int64_t)((uint64_t)RA.i * (uint64_t)RB.i);
    RV2_NEXT();
op_DIV_I:
    if (RB.i == 0 || (RA.i == INT64_MIN && RB.i == -1))
        RV2_MISSING();
    RD.i = RA.i / RB.i;
    RV2_NEXT();
op_MOD_I:
    if (RB.i == 0 || (RA.i == INT64_MIN && RB.i == -1))
        RV2_MISSING();
    RD.i = RA.i % RB.i;
    RV2_NEXT();
op_NEG_I:
    RD.i = (int64_t)(0 - (uint64_t)RA.i);
    RV2_NEXT();
op_ABS_I:
    RD.i = RA.i < 0 ? (int64_t)(0 - (uint64_t)RA.i) : RA.i;
    RV2_NEXT();
op_MIN_I:
    RD.i = RA.i < RB.i ? RA.i : RB.i;
    RV2_NEXT();
op_MAX_I:
    RD.i = RA.i > RB.i ? RA.i : RB.i;
    RV2_NEXT();

op_ADD_F:
    RD.f = RA.f + RB.f;
    RV2_NEXT();
op_SUB_F:
    RD.f = RA.f - RB.f;
    RV2_NEXT();
op_MUL_F:
    RD.f = RA.f * RB.f;
    RV2_NEXT();
op_DIV_F:
    RD.f = RA.f / RB.f;
    RV2_NEXT();
op_NEG_F:
    RD.f = -RA.f;
    RV2_NEXT();
op_ABS_F:
    RD.f = RA.f < 0 ? -RA.f : RA.f;
    RV2_NEXT();
op_MIN_F:
    RD.f = RA.f < RB.f ? RA.f : RB.f;
    RV2_NEXT();
op_MAX_F:
    RD.f = RA.f > RB.f ? RA.f : RB.f;
    RV2_NEXT();

op_LEN:
    RD.i = strlen(RA.s);
    RV2_NEXT();
op_LOWER:
    RD.s = rv2_strcase(ctx, RA.s, TRUE);
    RV2_NEXT();
op_UPPER:
    RD.s = rv2_strcase(ctx, RA.s, FALSE);
    RV2_NEXT();

op_EQ_I:
    RV2_BRANCH(RA.i == RB.i);
op_LT_I:
    RV2_BRANCH(RA.i < RB.i);
op_LE_I:
    RV2_BRANCH(RA.i <= RB.i);
op_GT_I:
    RV2_BRANCH(RA.i > RB.i);
op_GE_I:
    RV2_BRANCH(RA.i >= RB.i);
op_EQ_F:
    RV2_BRANCH(RA.f == RB.f);
op_LT_F:
    RV2_BRANCH(RA.f < RB.f);
op_LE_F:
    RV2_BRANCH(RA.f <= RB.f);
op_GT_F:
    RV2_BRANCH(RA.f > RB.f);
op_GE_F:
    RV2_BRANCH(RA.f >= RB.f);
op_EQ_S:
    RV2_BRANCH(strcmp(RA.s, RB.s) == 0);
op_EQ_IP:
    RV2_BRANCH(memcmp(RA.ip, RB.ip, sizeof(struct in6_addr)) == 0);

op_MATCH_FIELD_INT:
    RV2_BRANCH(rv2_iter_int(ctx, insn->field, rv2_cb_intset, K.iset) == 1);
op_MATCH_FIELD_FLOAT:
    RV2_BRANCH(rv2_iter_float(ctx, insn->field, rv2_cb_floatset, K.fset) == 1);
op_MATCH_FIELD_STR:
    RV2_BRANCH(rv2_iter_str(ctx, insn->field, rv2_cb_strset, K.sset) == 1);
op_MATCH_FIELD_IP:
    RV2_BRANCH(rv2_iter_ip(ctx, insn->field, rv2_cb_ipset, K.ipset) == 1);
op_MATCH_INT:
    RV2_BRANCH(rv2_intset_match(K.iset, RA.i));
op_MATCH_FLOAT:
    RV2_BRANCH(rv2_floatset_match(K.fset, RA.f));
op_MATCH_STR:
    RV2_BRANCH(rv2_strset_match(K.sset, RA.s));
op_MATCH_IP:
    RV2_BRANCH(rv2_ipset_match(K.ipset, RA.ip));

op_EXISTS:
    RV2_BRANCH(rv2_exists(ctx, insn->field));
op_STARTS:
    RV2_BRANCH(strncmp(RA.s, K.s.str, K.s.len) == 0);
op_ENDS: {
    const int len = strlen(RA.s);
    RV2_BRANCH(len >= K.s.len && memcmp(RA.s + len - K.s.len, K.s.str, K.s.len) == 0);
}
op_CONTAINS:
    RV2_BRANCH(arkime_memstr(RA.s, strlen(RA.s), K.s.str, K.s.len) != NULL);
op_ICONTAINS:
    RV2_BRANCH(rv2_icontains(RA.s, K.s.str, K.s.len));

op_IS_PRIVATE:
    RV2_BRANCH(rv2_ip_private(RA.ip));
op_IS_MULTICAST:
    RV2_BRANCH(rv2_ip_multicast(RA.ip));
op_IS_V4:
    RV2_BRANCH(IN6_IS_ADDR_V4MAPPED(RA.ip));
op_IS_V6:
    RV2_BRANCH(!IN6_IS_ADDR_V4MAPPED(RA.ip));

op_BPF:
    RV2_BRANCH(rv2_bpf_match(ctx, K.bpf));
op_BOOL:
    RV2_BRANCH(insn->b);
op_CALL_I:
op_CALL_F:
op_CALL_S:
op_CALL_IP:
    if (!rv2_call(ctx, K.call, &RD))
        RV2_MISSING();
    RV2_NEXT();
op_CALL_B: {
    RV2Reg_t result;
    const gboolean ok = rv2_call(ctx, K.call, &result);
    RV2_BRANCH(ok && result.i);
}
op_ANY:
    RV2_BRANCH(rv2_iter_sub(ctx, insn, FALSE) == 1);
op_ALL:
    RV2_BRANCH(rv2_iter_sub(ctx, insn, TRUE) == 0);

#undef RV2_DISPATCH
#undef RV2_NEXT
#undef RV2_BRANCH
#undef RV2_MISSING
#undef RV2_JUMP
#undef RA
#undef RB
#undef RD
#undef K
}
/******************************************************************************/
/******************************************************************************/
/* Small interpreter for programs that are only field tests, the common shape
 * for fields: rules and simple expressions. The full VM's prologue saves
 * every callee saved register, which costs more than one of these tests. */
/* Out of line so the small interpreter keeps a small frame. Internal int and ip
 * getCbs just read the session, so these skip the getCb cache, which only pays
 * off when a field is read more than once. */
LOCAL __attribute__((noinline)) gboolean rv2_test_int(RV2Ctx_t *ctx, int pos, void *set)
{
    const ArkimeFieldInfo_t *info = config.fields[pos];
    if (info->cntForPos)
        return rv2_intset_match(set, rv2_count(ctx, info->cntForPos));
    if (pos >= config.minInternalField && info->type == ARKIME_FIELD_TYPE_INT && info->getCb && pos != ctx->triggerField)
        return rv2_intset_match(set, (long)info->getCb(ctx->session, pos));
    return rv2_iter_int(ctx, pos, rv2_cb_intset, set) == 1;
}
LOCAL __attribute__((noinline)) gboolean rv2_test_float(RV2Ctx_t *ctx, int pos, void *set)
{
    return rv2_iter_float(ctx, pos, rv2_cb_floatset, set) == 1;
}
LOCAL __attribute__((noinline)) gboolean rv2_test_str(RV2Ctx_t *ctx, int pos, void *set)
{
    return rv2_iter_str(ctx, pos, rv2_cb_strset, set) == 1;
}
LOCAL __attribute__((noinline)) gboolean rv2_test_ip(RV2Ctx_t *ctx, int pos, void *set)
{
    const ArkimeFieldInfo_t *info = config.fields[pos];
    if (pos >= config.minInternalField && info->type == ARKIME_FIELD_TYPE_IP && info->getCb && pos != ctx->triggerField) {
        const struct in6_addr *ip = info->getCb(ctx->session, pos);
        return ip && rv2_ipset_match(set, ip);
    }
    return rv2_iter_ip(ctx, pos, rv2_cb_ipset, set) == 1;
}
/******************************************************************************/
// A plain session field that isn't set, the most common way a test fails
RV2_INLINE gboolean rv2_field_unset(const RV2Ctx_t *ctx, int pos)
{
    return pos < config.minInternalField && !config.fields[pos]->cntForPos &&
           (pos >= ctx->session->maxFields || !ctx->session->fields[pos]);
}
/******************************************************************************/
LOCAL gboolean rv2_exec_tests(RV2Ctx_t *ctx, int pc)
{
    const RV2Insn_t *const insns = ctx->prog->blocks[0].insns;
    const RV2Const_t *const consts = ctx->prog->consts;
    const RV2Insn_t *insn = insns + pc;

    while (1) {
        gboolean r;
        switch (insn->op) {
        case RV2_OP_MATCH_FIELD_INT:
            r = !rv2_field_unset(ctx, insn->field) && rv2_test_int(ctx, insn->field, consts[insn->k].iset);
            break;
        case RV2_OP_MATCH_FIELD_FLOAT:
            r = !rv2_field_unset(ctx, insn->field) && rv2_test_float(ctx, insn->field, consts[insn->k].fset);
            break;
        case RV2_OP_MATCH_FIELD_STR:
            r = !rv2_field_unset(ctx, insn->field) && rv2_test_str(ctx, insn->field, consts[insn->k].sset);
            break;
        case RV2_OP_MATCH_FIELD_IP:
            r = !rv2_field_unset(ctx, insn->field) && rv2_test_ip(ctx, insn->field, consts[insn->k].ipset);
            break;
        case RV2_OP_EXISTS:
            r = rv2_exists(ctx, insn->field);
            break;
        case RV2_OP_BOOL:
            r = insn->b;
            break;
        case RV2_OP_RET_TRUE:
            return TRUE;
        default:
            return FALSE;
        }

        if (G_UNLIKELY(ctx->explain))
            rv2_explain(ctx, insn, r);
        const uint16_t target = r ? insn->jt : insn->jf;
        if (target >= RV2_JUMP_RET_FALSE)
            return target == RV2_JUMP_RET_TRUE;
        insn = insns + target;
    }
}
/******************************************************************************/
LOCAL gboolean rv2_eval(const ArkimeRulesV2Prog_t *prog, ArkimeSession_t *session, ArkimePacket_t *packet, GString *explain,
                         int triggerField, const void *triggerValue, int pc)
{
    RV2Ctx_t ctx;
    ctx.triggerField = triggerField;
    ctx.triggerValue = triggerValue;
    ctx.session = session;
    ctx.packet = packet;
    ctx.prog = prog;
    ctx.heap = NULL;
    ctx.explain = explain;
    ctx.scratchUsed = 0;
    ctx.ngetcb = 0;

    const gboolean result = prog->testsOnly ? rv2_exec_tests(&ctx, pc) : rv2_exec(&ctx, 0);

    if (ctx.heap)
        g_ptr_array_free(ctx.heap, TRUE);
    return result;
}
/******************************************************************************/
LOCAL __attribute__((noinline)) int rv2_first_target_ctx(const ArkimeRulesV2Prog_t *prog, ArkimeSession_t *session, int triggerField, const void *triggerValue)
{
    const RV2Insn_t *insn = prog->blocks[0].insns;
    RV2Ctx_t ctx;
    ctx.session = session;
    ctx.prog = prog;
    ctx.packet = NULL;
    ctx.heap = NULL;
    ctx.explain = NULL;
    ctx.scratchUsed = 0;
    ctx.triggerField = triggerField;
    ctx.triggerValue = triggerValue;
    ctx.ngetcb = 0;

    gboolean r;
    switch (insn->op) {
    case RV2_OP_MATCH_FIELD_INT:
        r = rv2_test_int(&ctx, insn->field, prog->consts[insn->k].iset);
        break;
    case RV2_OP_MATCH_FIELD_FLOAT:
        r = rv2_test_float(&ctx, insn->field, prog->consts[insn->k].fset);
        break;
    case RV2_OP_MATCH_FIELD_STR:
        r = rv2_test_str(&ctx, insn->field, prog->consts[insn->k].sset);
        break;
    case RV2_OP_MATCH_FIELD_IP:
        r = rv2_test_ip(&ctx, insn->field, prog->consts[insn->k].ipset);
        break;
    default:
        // EXISTS, which has no const
        r = rv2_exists(&ctx, insn->field);
        break;
    }
    return r ? insn->jt : insn->jf;
}
/******************************************************************************/
/* Run the first field test without the interpreter's setup, and return its
 * jump target. Most field test programs end on their first test, one field
 * rules always and && rules usually. Internal int and ip fields just read the
 * session, so they don't need a context at all. No explain, that goes through
 * rv2_eval. */
LOCAL int rv2_first_target(const ArkimeRulesV2Prog_t *prog, ArkimeSession_t *session, int triggerField, const void *triggerValue)
{
    const RV2Insn_t *insn = prog->blocks[0].insns;
    if (insn->field >= config.minInternalField && insn->field != triggerField) {
        const ArkimeFieldInfo_t *info = config.fields[insn->field];
        if (insn->op == RV2_OP_MATCH_FIELD_IP && info->type == ARKIME_FIELD_TYPE_IP && info->getCb) {
            const struct in6_addr *ip = info->getCb(session, insn->field);
            return ip && rv2_ipset_match(prog->consts[insn->k].ipset, ip) ? insn->jt : insn->jf;
        }
        if (insn->op == RV2_OP_MATCH_FIELD_INT && info->type == ARKIME_FIELD_TYPE_INT && info->getCb)
            return rv2_intset_match(prog->consts[insn->k].iset, (long)info->getCb(session, insn->field)) ? insn->jt : insn->jf;
    }

    // .cnt of a non internal field is just a count of the session's field
    if (insn->op == RV2_OP_MATCH_FIELD_INT) {
        const int cntFor = config.fields[insn->field]->cntForPos;
        if (cntFor && cntFor < config.minInternalField)
            return rv2_intset_match(prog->consts[insn->k].iset, rv2_session_count(session, cntFor)) ? insn->jt : insn->jf;
    }
    return rv2_first_target_ctx(prog, session, triggerField, triggerValue);
}
/******************************************************************************/
LOCAL gboolean rv2_eval_first(const ArkimeRulesV2Prog_t *prog, ArkimeSession_t *session, ArkimePacket_t *packet, int triggerField, const void *triggerValue)
{
    const int target = rv2_first_target(prog, session, triggerField, triggerValue);
    if (target >= RV2_JUMP_RET_FALSE)
        return target == RV2_JUMP_RET_TRUE;
    return rv2_eval(prog, session, packet, NULL, triggerField, triggerValue, target);
}
/******************************************************************************/
/* Most rules fail their first test on a field the session doesn't have. Only
 * plain session fields, which already have a value that triggered a fieldSet. */
#define RV2_QUICK(prog, session) \
    ((prog)->quickField >= 0 && ((prog)->quickField >= (session)->maxFields || !(session)->fields[(prog)->quickField]))

gboolean arkime_rulesv2_eval(const ArkimeRulesV2Prog_t *prog, ArkimeSession_t *session, ArkimePacket_t *packet)
{
    if (RV2_QUICK(prog, session))
        return prog->quickResult;
    if (prog->firstTest)
        return rv2_eval_first(prog, session, packet, -1, NULL);
    return rv2_eval(prog, session, packet, NULL, -1, NULL, 0);
}
/******************************************************************************/
/* For fieldSet, field was just set to value, in the form passed to
 * arkime_rules_run_field_set. Internal fields like cert.alt that can't be read
 * until parsing finishes see value as one of theirs. */
gboolean arkime_rulesv2_eval_set(const ArkimeRulesV2Prog_t *prog, ArkimeSession_t *session, int field, const void *value)
{
    if (RV2_QUICK(prog, session))
        return prog->quickResult;
    if (prog->firstTest)
        return rv2_eval_first(prog, session, NULL, field, value);
    return rv2_eval(prog, session, NULL, NULL, field, value, 0);
}
/******************************************************************************/
/* Appends what made the expression match, as "condition (value), ...". For
 * fieldSet pass the field and value like arkime_rulesv2_eval_set, else -1. */
void arkime_rulesv2_explain(const ArkimeRulesV2Prog_t *prog, ArkimeSession_t *session, ArkimePacket_t *packet,
                            int triggerField, const void *triggerValue, GString *out)
{
    const gsize start = out->len;
    rv2_eval(prog, session, packet, out, triggerField, triggerValue, 0);
    if (out->len >= start + 2)
        g_string_truncate(out, out->len - 2);
}
/******************************************************************************/
void arkime_rulesv2_recompile(ArkimeRulesV2Prog_t *prog, const char *filename)
{
    for (int k = 0; k < prog->nconsts; k++) {
        if (prog->consts[k].kind == RV2_K_BPF)
            arkime_rules_bpf_compile(filename, prog->consts[k].bpf->filter, prog->consts[k].bpf->bpfp);
    }
}
/******************************************************************************/
/* fieldSet triggers. Fields only ever gain values, so an expression can only
 * become true when a field used without a ! gets a new value. Every such field
 * is a trigger, not just one per &&, so the order fields are set in doesn't
 * matter, same as fields: rules indexing every field. Matches directly on a field
 * trigger only on their values. count(), .cnt and all() can also become true
 * when a field under a ! gets a value, so those trigger in either polarity. */
/******************************************************************************/
// Internal fields only trigger if something calls ARKIME_RULES_RUN_FIELD_SET for them
LOCAL gboolean rv2_triggerable(int pos)
{
    return pos < config.minInternalField || config.fields[pos]->runsFieldSet;
}
/******************************************************************************/
LOCAL void rv2_trigger_any(GArray *triggers, int pos)
{
    if (config.fields[pos]->cntForPos)
        pos = config.fields[pos]->cntForPos;
    if (!rv2_triggerable(pos))
        return;

    ArkimeRulesV2Trigger_t trigger;
    memset(&trigger, 0, sizeof(trigger));
    trigger.kind = ARKIME_RULESV2_TRIGGER_ANY;
    trigger.field = pos;
    g_array_append_val(triggers, trigger);
}
/******************************************************************************/
// All fields used anywhere in the subtree
LOCAL void rv2_trigger_fields(GArray *triggers, const ArkimeRulesV2Node_t *node)
{
    if (node->type == RV2_NODE_FIELD || node->type == RV2_NODE_EXISTS)
        rv2_trigger_any(triggers, node->field);
    for (int k = 0; k < RV2_NKIDS(node); k++)
        rv2_trigger_fields(triggers, RV2_KID(node, k));
}
/******************************************************************************/
LOCAL gboolean rv2_non_monotone(const ArkimeRulesV2Node_t *node)
{
    if (node->type == RV2_NODE_FIELD && config.fields[node->field]->cntForPos)
        return TRUE;
    if (rv2_is_call(node, "count") || rv2_is_call(node, "all"))
        return TRUE;
    for (int k = 0; k < RV2_NKIDS(node); k++) {
        if (rv2_non_monotone(RV2_KID(node, k)))
            return TRUE;
    }
    return FALSE;
}
/******************************************************************************/
// Index on the match values when they all can be, else any value of the field
LOCAL void rv2_trigger_match(GArray *triggers, const ArkimeRulesV2Node_t *node, int pos)
{
    GArray *values = g_array_new(FALSE, TRUE, sizeof(ArkimeRulesV2Trigger_t));

    for (int v = 0; v < node->nvalues; v++) {
        const RV2Value_t *value = &node->values[v];
        ArkimeRulesV2Trigger_t trigger;
        memset(&trigger, 0, sizeof(trigger));
        trigger.field = pos;

        switch (value->kind) {
        case RV2_VAL_INT:
        case RV2_VAL_INT_RANGE:
            if (value->imax - value->imin < 20) {
                trigger.kind = ARKIME_RULESV2_TRIGGER_INT;
                for (int64_t i = value->imin; i <= value->imax; i++) {
                    trigger.imin = trigger.imax = i;
                    g_array_append_val(values, trigger);
                }
                continue;
            }
            if (value->imin < 0 || value->imax > UINT32_MAX)
                goto any;
            trigger.kind = ARKIME_RULESV2_TRIGGER_INT_RANGE;
            trigger.imin = value->imin;
            trigger.imax = value->imax;
            break;
        case RV2_VAL_FLOAT:
            trigger.kind = ARKIME_RULESV2_TRIGGER_FLOAT;
            trigger.f = value->fmin;
            break;
        case RV2_VAL_STR:
        case RV2_VAL_HEAD:
        case RV2_VAL_TAIL:
        case RV2_VAL_CONTAINS:
            if (value->kind != RV2_VAL_STR && value->len > 255)
                goto any;
            trigger.kind = value->kind == RV2_VAL_STR ? ARKIME_RULESV2_TRIGGER_STR :
                           value->kind == RV2_VAL_HEAD ? ARKIME_RULESV2_TRIGGER_HEAD :
                           value->kind == RV2_VAL_TAIL ? ARKIME_RULESV2_TRIGGER_TAIL : ARKIME_RULESV2_TRIGGER_CONTAINS;
            trigger.str = value->str;
            break;
        case RV2_VAL_CIDR: {
            char str[INET6_ADDRSTRLEN + 5];
            if (value->family == 4)
                inet_ntop(AF_INET, value->ip.s6_addr + 12, str, INET6_ADDRSTRLEN);
            else
                inet_ntop(AF_INET6, &value->ip, str, INET6_ADDRSTRLEN);
            snprintf(str + strlen(str), 5, "/%u", value->bits);
            trigger.kind = ARKIME_RULESV2_TRIGGER_CIDR;
            trigger.family = value->family;
            trigger.str = g_strdup(str);
            break;
        }
        default:
            goto any;
        }
        g_array_append_val(values, trigger);
    }

    for (guint t = 0; t < values->len; t++) {
        ArkimeRulesV2Trigger_t *trigger = &g_array_index(values, ArkimeRulesV2Trigger_t, t);
        if (trigger->kind >= ARKIME_RULESV2_TRIGGER_STR && trigger->kind != ARKIME_RULESV2_TRIGGER_CIDR)
            trigger->str = g_strdup(trigger->str);
        g_array_append_val(triggers, *trigger);
    }
    g_array_free(values, TRUE);
    return;

any:
    for (guint t = 0; t < values->len; t++) {
        if (g_array_index(values, ArkimeRulesV2Trigger_t, t).kind == ARKIME_RULESV2_TRIGGER_CIDR)
            g_free(g_array_index(values, ArkimeRulesV2Trigger_t, t).str);
    }
    g_array_free(values, TRUE);
    rv2_trigger_any(triggers, pos);
}
/******************************************************************************/
LOCAL void rv2_triggers(GArray *triggers, const ArkimeRulesV2Node_t *node, gboolean positive)
{
    switch (node->type) {
    case RV2_NODE_AND:
    case RV2_NODE_OR:
        for (int k = 0; k < RV2_NKIDS(node); k++)
            rv2_triggers(triggers, RV2_KID(node, k), positive);
        return;

    case RV2_NODE_NOT:
        rv2_triggers(triggers, RV2_KID(node, 0), !positive);
        return;

    default:
        break;
    }

    if (!positive && !rv2_non_monotone(node))
        return;

    const ArkimeRulesV2Node_t *lhs = RV2_NKIDS(node) ? RV2_KID(node, 0) : NULL;
    if (positive && node->type == RV2_NODE_MATCH && lhs->type == RV2_NODE_FIELD && !config.fields[lhs->field]->cntForPos) {
        if (rv2_triggerable(lhs->field))
            rv2_trigger_match(triggers, node, lhs->field);
        return;
    }

    rv2_trigger_fields(triggers, node);
}
/******************************************************************************/
LOCAL char *rv2_trigger_key(const ArkimeRulesV2Trigger_t *trigger)
{
    switch (trigger->kind) {
    case ARKIME_RULESV2_TRIGGER_ANY:
        return g_strdup_printf("%d:%d", trigger->field, trigger->kind);
    case ARKIME_RULESV2_TRIGGER_INT:
    case ARKIME_RULESV2_TRIGGER_INT_RANGE:
        return g_strdup_printf("%d:%d:%" PRId64 ":%" PRId64, trigger->field, trigger->kind, trigger->imin, trigger->imax);
    case ARKIME_RULESV2_TRIGGER_FLOAT:
        return g_strdup_printf("%d:%d:%a", trigger->field, trigger->kind, trigger->f);
    default:
        return g_strdup_printf("%d:%d:%s", trigger->field, trigger->kind, trigger->str);
    }
}
/******************************************************************************/
// Drop duplicates, and specific values of a field that also triggers on any value
LOCAL void rv2_triggers_finish(GArray *triggers)
{
    GHashTable *anyFields = g_hash_table_new(NULL, NULL);
    GHashTable *seen = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

    for (guint t = 0; t < triggers->len; t++) {
        const ArkimeRulesV2Trigger_t *trigger = &g_array_index(triggers, ArkimeRulesV2Trigger_t, t);
        if (trigger->kind == ARKIME_RULESV2_TRIGGER_ANY)
            g_hash_table_add(anyFields, GINT_TO_POINTER(trigger->field));
    }

    guint o = 0;
    for (guint t = 0; t < triggers->len; t++) {
        ArkimeRulesV2Trigger_t *trigger = &g_array_index(triggers, ArkimeRulesV2Trigger_t, t);
        const gboolean covered = trigger->kind != ARKIME_RULESV2_TRIGGER_ANY &&
                                 g_hash_table_contains(anyFields, GINT_TO_POINTER(trigger->field));
        if (covered || !g_hash_table_add(seen, rv2_trigger_key(trigger))) {
            if (trigger->kind >= ARKIME_RULESV2_TRIGGER_STR)
                g_free(trigger->str);
            continue;
        }
        g_array_index(triggers, ArkimeRulesV2Trigger_t, o++) = *trigger;
    }
    g_array_set_size(triggers, o);

    g_hash_table_destroy(anyFields);
    g_hash_table_destroy(seen);
}
/******************************************************************************/
int arkime_rulesv2_triggers(const ArkimeRulesV2Prog_t *prog, const ArkimeRulesV2Trigger_t **triggers)
{
    *triggers = prog->triggers;
    return prog->ntriggers;
}
/******************************************************************************/
LOCAL void rv2_print_triggers(const ArkimeRulesV2Prog_t *prog, GString *out)
{
    static const char *kinds[] = {"any", "int", "range", "float", "str", "head", "tail", "contains", "cidr"};
    g_string_append(out, "triggers:");
    for (int t = 0; t < prog->ntriggers; t++) {
        const ArkimeRulesV2Trigger_t *trigger = &prog->triggers[t];
        g_string_append_printf(out, " %s:%s", config.fields[trigger->field]->expression, kinds[trigger->kind]);
        switch (trigger->kind) {
        case ARKIME_RULESV2_TRIGGER_ANY:
            break;
        case ARKIME_RULESV2_TRIGGER_INT:
            g_string_append_printf(out, ":%" PRId64, trigger->imin);
            break;
        case ARKIME_RULESV2_TRIGGER_INT_RANGE:
            g_string_append_printf(out, ":%" PRId64 "-%" PRId64, trigger->imin, trigger->imax);
            break;
        case ARKIME_RULESV2_TRIGGER_FLOAT:
            g_string_append_printf(out, ":%g", trigger->f);
            break;
        default:
            g_string_append_printf(out, ":%s", trigger->str);
            break;
        }
    }
    g_string_append_c(out, '\n');
}
/******************************************************************************/
/* Shared backend for expressions and fields: rules, takes ownership of
 * the checked tree. source is what the tree's spans point into. */
LOCAL ArkimeRulesV2Prog_t *rv2_compile_tree(const char *filename, const char *ruleName, int ruleType, const char *source,
                                            ArkimeRulesV2Node_t *root, gboolean wantTriggers, const char *label)
{
    GString *ast = NULL;
    if (config.debug > 1) {
        ast = g_string_new(NULL);
        arkime_rulesv2_print(root, ast);
    }

    RV2Parser_t p;
    memset(&p, 0, sizeof(p));
    p.src = source;
    p.len = strlen(source);

    root = rv2_lower(&p, root, 0);
    if (!p.error)
        root = rv2_optimize(root);

    if (ast) {
        g_string_append(ast, "\noptimized: ");
        arkime_rulesv2_print(root, ast);
    }
    if (root->type == RV2_NODE_BOOL && ruleType != ARKIME_RULE_TYPE_FIELD_SET)
        LOG("WARNING - %s: rule '%s' %s is always %s", filename, ruleName, label, root->i ? "true" : "false");

    RV2Gen_t gen;
    memset(&gen, 0, sizeof(gen));
    gen.p = &p;
    gen.blocks = g_array_new(FALSE, FALSE, sizeof(RV2Block_t));
    gen.consts = g_array_new(FALSE, FALSE, sizeof(RV2Const_t));
    gen.cseNodes = g_ptr_array_new();
    gen.cseRegs = g_array_new(FALSE, FALSE, sizeof(int));
    gen.nextReg = gen.maxReg = RV2_MAX_DEPTH;

    if (!p.error)
        rv2_gen_block(&gen, root, 0);

    ArkimeRulesV2Prog_t *prog = g_new0(ArkimeRulesV2Prog_t, 1);
    prog->nblocks = gen.blocks->len;
    prog->blocks = (RV2Block_t *)g_array_free(gen.blocks, FALSE);
    prog->nconsts = gen.consts->len;
    prog->consts = (RV2Const_t *)g_array_free(gen.consts, FALSE);
    prog->nregs = gen.maxReg;
    prog->source = g_strdup(source);
    prog->sourceLen = strlen(source);
    g_ptr_array_free(gen.cseNodes, TRUE);
    g_array_free(gen.cseRegs, TRUE);

    if (wantTriggers && !p.error) {
        GArray *triggers = g_array_new(FALSE, TRUE, sizeof(ArkimeRulesV2Trigger_t));
        rv2_triggers(triggers, root, TRUE);
        rv2_triggers_finish(triggers);
        prog->ntriggers = triggers->len;
        prog->triggers = (ArkimeRulesV2Trigger_t *)g_array_free(triggers, FALSE);
        if (prog->ntriggers == 0)
            CONFIGEXIT("%s: rule '%s' fieldSet expression has nothing to trigger on, it needs a condition not under ! on a field that is set while parsing, such as ip.src, port.dst, cert.alt or any non internal field, internal fields like packets.src aren't", filename, ruleName);
    }
    arkime_rulesv2_free(root);

    if (p.error)
        CONFIGEXIT("%s: rule '%s' %s error: %s", filename, ruleName, label, rv2_format_error(&p));

    char *error = rv2_verify(prog);
    if (error)
        CONFIGEXIT("%s: rule '%s' internal error, %s program failed verification: %s", filename, ruleName, label, error);

    /* When the first test's field, or for .cnt the field it counts, isn't set
     * the test's result is known: false, or for .cnt whether 0 matches. If that
     * result returns, eval can answer from the session's field slot alone. */
    prog->quickField = -1;
    const RV2Insn_t *first = &prog->blocks[0].insns[0];
    if ((first->op >= RV2_OP_MATCH_FIELD_INT && first->op <= RV2_OP_MATCH_FIELD_IP) || first->op == RV2_OP_EXISTS) {
        const int cntFor = config.fields[first->field]->cntForPos;
        const int unsetPos = cntFor ? cntFor : first->field;
        gboolean unsetResult = FALSE;
        if (cntFor && first->op == RV2_OP_MATCH_FIELD_INT)
            unsetResult = rv2_intset_match(prog->consts[first->k].iset, 0);
        const int target = unsetResult ? first->jt : first->jf;
        if (unsetPos < config.minInternalField && !config.fields[unsetPos]->cntForPos && target >= RV2_JUMP_RET_FALSE) {
            prog->quickField = unsetPos;
            prog->quickResult = target == RV2_JUMP_RET_TRUE;
        }
    }

    prog->testsOnly = prog->nblocks == 1;
    for (int pc = 0; pc < prog->blocks[0].ninsns && prog->testsOnly; pc++) {
        switch (prog->blocks[0].insns[pc].op) {
        case RV2_OP_MATCH_FIELD_INT:
        case RV2_OP_MATCH_FIELD_FLOAT:
        case RV2_OP_MATCH_FIELD_STR:
        case RV2_OP_MATCH_FIELD_IP:
        case RV2_OP_EXISTS:
        case RV2_OP_BOOL:
        case RV2_OP_RET_TRUE:
        case RV2_OP_RET_FALSE:
            break;
        default:
            prog->testsOnly = FALSE;
        }
    }
    prog->firstTest = prog->testsOnly && ((first->op >= RV2_OP_MATCH_FIELD_INT && first->op <= RV2_OP_MATCH_FIELD_IP) || first->op == RV2_OP_EXISTS);

    if (ast) {
        GString *dis = g_string_new(NULL);
        arkime_rulesv2_disassemble(prog, dis);
        if (prog->ntriggers)
            rv2_print_triggers(prog, dis);
        LOG("%s: rule '%s' %s %s\n%s", filename, ruleName, label, ast->str, dis->str);
        g_string_free(dis, TRUE);
        g_string_free(ast, TRUE);
    }

    return prog;
}
/******************************************************************************/
ArkimeRulesV2Prog_t *arkime_rulesv2_compile(const char *filename, const char *ruleName, int ruleType, const char *expression)
{
    char *error = NULL;
    ArkimeRulesV2Node_t *root = arkime_rulesv2_parse(expression, ruleType, &error);
    if (!root)
        CONFIGEXIT("%s: rule '%s' expression error: %s", filename, ruleName, error);

    return rv2_compile_tree(filename, ruleName, ruleType, expression, root, ruleType == ARKIME_RULE_TYPE_FIELD_SET, "expression");
}
/******************************************************************************/
/* fields: rules, built directly as a tree since their values are literal, foo*
 * isn't a wildcard, and parsed the way they always have been: atoi, ranges, no
 * case changes. The fields are an && of a match on each field's values, and
 * !field is a ! of the match. */
/******************************************************************************/
struct arkime_rulesv2_fields {
    ArkimeRulesV2Node_t *root;
    GHashTable          *matches;   // pos * 2 + negated to its match node
};
/******************************************************************************/
ArkimeRulesV2Fields_t *arkime_rulesv2_fields_new()
{
    ArkimeRulesV2Fields_t *builder = g_new0(ArkimeRulesV2Fields_t, 1);
    builder->root = rv2_node_new(RV2_NODE_AND, 0, 0);
    builder->root->vtype = RV2_VT_BOOL;
    builder->matches = g_hash_table_new(NULL, NULL);
    return builder;
}
/******************************************************************************/
LOCAL void rv2_fields_add_value(ArkimeRulesV2Node_t *match, const RV2Value_t *value)
{
    match->values = g_renew(RV2Value_t, match->values, match->nvalues + 1);
    match->values[match->nvalues++] = *value;
}
/******************************************************************************/
void arkime_rulesv2_fields_add(ArkimeRulesV2Fields_t *builder, int pos, gboolean negated, int matchType, const char *key)
{
    const int hkey = pos * 2 + (negated ? 1 : 0);
    ArkimeRulesV2Node_t *match = g_hash_table_lookup(builder->matches, GINT_TO_POINTER(hkey));
    if (!match) {
        match = rv2_node_new(RV2_NODE_MATCH, 0, 0);
        match->op = RV2_TOK_EQ;
        match->vtype = RV2_VT_BOOL;
        ArkimeRulesV2Node_t *field = rv2_node_new(RV2_NODE_FIELD, 0, 0);
        field->field = pos;
        field->vtype = rv2_field_vtype(pos, &field->multi);
        field->str = g_strdup(config.fields[pos]->expression);
        rv2_node_add_kid(match, field);
        g_hash_table_insert(builder->matches, GINT_TO_POINTER(hkey), match);
        rv2_node_add_kid(builder->root, negated ? rv2_wrap_not(match) : match);
    }

    RV2Value_t value;
    memset(&value, 0, sizeof(value));

    switch (RV2_KID(match, 0)->vtype) {
    case RV2_VT_INT: {
        // Values have always compared as uint32, so a negative value also matches its uint32 form
        const char *dash = negated || key[0] == '-' ? NULL : strchr(key, '-');
        if (dash) {
            value.kind = RV2_VAL_INT_RANGE;
            value.imin = (uint32_t)arkime_atoin(key, dash - key);
            value.imax = (uint32_t)atoi(dash + 1);
            rv2_fields_add_value(match, &value);
            break;
        }
        const int n = atoi(key);
        value.kind = RV2_VAL_INT;
        value.imin = value.imax = (uint32_t)n;
        rv2_fields_add_value(match, &value);
        if (n < 0) {
            value.imin = value.imax = n;
            rv2_fields_add_value(match, &value);
        }
        break;
    }
    case RV2_VT_FLOAT:
        value.kind = RV2_VAL_FLOAT;
        value.fmin = value.fmax = atof(key);
        rv2_fields_add_value(match, &value);
        break;
    case RV2_VT_IP: {
        RV2Parser_t p;
        memset(&p, 0, sizeof(p));
        if (rv2_parse_value_ip(&p, &value, key, 0))
            rv2_fields_add_value(match, &value);
        g_free(p.error);
        break;
    }
    case RV2_VT_STR:
        value.kind = matchType == ARKIME_RULESV2_FIELDS_HEAD ? RV2_VAL_HEAD :
                     matchType == ARKIME_RULESV2_FIELDS_TAIL ? RV2_VAL_TAIL :
                     matchType == ARKIME_RULESV2_FIELDS_CONTAINS ? RV2_VAL_CONTAINS : RV2_VAL_STR;
        value.str = g_strdup(key);
        value.len = strlen(key);
        rv2_fields_add_value(match, &value);
        break;
    default:
        break;
    }
}
/******************************************************************************/
/* Frees builder, returns NULL if the rule has no fields. The source text is the
 * field names, so log: true shows field (value) for each field that matched. */
ArkimeRulesV2Prog_t *arkime_rulesv2_fields_compile(ArkimeRulesV2Fields_t *builder, const char *filename, const char *ruleName, int ruleType)
{
    ArkimeRulesV2Node_t *root = builder->root;
    g_hash_table_destroy(builder->matches);
    g_free(builder);

    if (RV2_NKIDS(root) == 0) {
        arkime_rulesv2_free(root);
        return NULL;
    }

    GString *source = g_string_new(NULL);
    for (int k = 0; k < RV2_NKIDS(root); k++) {
        ArkimeRulesV2Node_t *match = RV2_KID(root, k);
        if (match->type != RV2_NODE_MATCH)
            continue;   // ! fields aren't logged
        if (source->len)
            g_string_append_c(source, ' ');
        match->start = source->len;
        g_string_append(source, config.fields[RV2_KID(match, 0)->field]->expression);
        match->end = source->len;
    }

    ArkimeRulesV2Prog_t *prog = rv2_compile_tree(filename, ruleName, ruleType, source->str, root, FALSE, "fields");
    g_string_free(source, TRUE);
    return prog;
}
