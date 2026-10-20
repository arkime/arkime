# Capture Rules Expressions

Rules files (`rulesFiles=`) can match sessions with an `expression:` instead of
`fields:` or `bpf:`. Expressions use the same syntax as the viewer search bar, plus
math and functions, and are compiled into bytecode when the rules load.
All three kinds can be mixed in the same `version: 1` file. `fields:` rules keep their
syntax and meaning, and are compiled onto the same engine (see below).

```yaml
version: 1
rules:
  - name: "Odd TLS"
    when: "afterClassify"
    expression: >
      protocols == tls
      && port.dst != [443, 8443]
      && !(ip.dst == [10.0.0.0/8, 192.168.0.0/16])
    log: true
    ops:
      tags: "odd-tls"

  - name: "Big upload"
    when: "beforeFinalSave"
    expression: "databytes.src > 10 * databytes.dst && databytes.src >= 50000000"
    ops:
      tags: "upload"
```

Each rule has exactly one of `fields:`, `bpf:` or `expression:`. `when:` and `ops:` work the
same as for other rules. `everyPacket` rules only support `bpf:`.

## Syntax

| | |
|---|---|
| `a && b`, `a & b` | and |
| `a \|\| b`, `a \| b` | or, `&&` binds tighter |
| `!a` | not |
| `( )` | grouping |
| `==` `=` `!=` | match a field against values |
| `<` `<=` `>` `>=` | numeric compare |
| `+` `-` `*` `/` `%` | math, needs spaces around the operator |

Field names use the same expressions as the viewer, for example `host.http`, `ip.dst`,
`port.src`, or `db:` plus the database field name. Field names and values can contain
`- + * /`, which is why math operators need spaces: `a * 2` works, `a*2` doesn't. A
leading `-` on a field negates it (`-databytes.src`).

### Values on the right of `==` and `!=`

| Form | Example |
|---|---|
| exact | `host.http == www.example.com`, `http.method == "GET"` |
| any of a list | `port.dst == [80, 8080, 8000-8100]` |
| all of a list | `http.method == ]GET,HEAD[` |
| wildcard | `host.http == *.example.com`, `foo*`, `*bar*`, `a?c*` |
| regex | `http.uri == /\/admin\/.*\.php/` matches the whole value, like the viewer |
| range | `port.dst == 1000-2000` |
| cidr | `ip.src == 10.0.0.0/8`, `ip.dst == 2001:db8::/32`, `ip.dst == ipv4` |
| exists | `http.uri == EXISTS!`, `http.uri != EXISTS!` |

A bare word on the right is a value for string fields, same as the viewer. For other
types it is a value when it parses as one, otherwise a field, so `port.src == port.dst`
compares two fields. Quote strings that contain spaces or operators. Shortcuts
(`$name`) aren't supported in capture.

Fields that capture keeps lowercase (or uppercase) have their exact and wildcard values
converted to match, so `host.http == WWW.Example.com` still works. Regex values aren't
converted.

### Matching rules

- **Multi value fields** match if any value matches: `http.method == GET` is true when
  one of the methods is `GET`.
- **`!=` is "no value matches"**, the same as `!(a == b)`. A missing field matches `!=`
  and doesn't match `==`.
- **Missing values**: a comparison or function that needs a missing value is false, so
  under `!` it is true. Division by zero counts as missing.
- **`all()`** is false when the field has no values.
- Using a multi value field anywhere other than directly in a match, for example
  `lower(host.http) == x` or `http.statuscode >= 400`, checks each value and is true if any
  value is.

## Functions

| Function | |
|---|---|
| `count(field)` | number of values, same as the `.cnt` fields |
| `has(field)` | field has a value |
| `len(s)` | string length |
| `lower(s)`, `upper(s)` | change case |
| `startsWith(s, x)`, `endsWith(s, x)`, `contains(s, x)` | substring tests |
| `icontains(s, x)` | case insensitive contains |
| `isPrivate(ip)` | 10/8, 172.16/12, 192.168/16, 100.64/10, 127/8, 169.254/16, fc00::/7, fe80::/10, ::1 |
| `isMulticast(ip)`, `isV4(ip)`, `isV6(ip)` | |
| `min(a, b)`, `max(a, b)`, `abs(a)` | numbers |
| `any(field, condition)` | condition is true for some value, `_` is the value |
| `all(field, condition)` | condition is true for every value |
| `bpf("filter")` | the packet matches a bpf filter, `sessionSetup` rules only |

```
any(http.hasheader.src, startsWith(_, "x-"))
all(http.method, _ == [GET, HEAD]) && count(http.method) > 1
```

Plugins can add functions, see below.

## fieldSet rules

A `fieldSet` expression is checked each time a field it uses gets a new value. It isn't
checked for fields that are only used under `!`, because fields only gain values, so a
negated condition can only become false. Fields set in any order work:
`http.method == GET && host.http == *.example.com` is checked when either one is set.

Only fields that capture sets as it parses can trigger the check: any non internal
field, plus `ip.src`, `ip.dst`, `port.src` and `port.dst`. A fieldSet expression with
nothing to trigger on, such as only negations or only internal fields like
`packets.src`, fails to load. Use a stage like `beforeFinalSave` for those.

Direct matches such as `host.http == [a, *.b]` or `port.dst == 1000-2000` only check the
rule for matching values. Other uses check it for any new value of the field.

A rule can match several times as values are added and run its ops each time, same as
`fields:` rules.

## log: true

Expression rules log the conditions that matched, in the order they ran, with the
value that matched:

```
Odd TLS - 10.0.0.1:52925 => 10.0.0.2:8000 - port.dst != [443, 8443], !(ip.dst == [10.0.0.0/8, 192.168.0.0/16]), protocols == tls (tls)
```

A condition that passed on a branch that later failed (`a && b || c` with `b` false) is
listed too.

## Errors and debugging

Mistakes are caught when the rules load, with the rule name, line, column and a caret:

```
FATAL CONFIG ERROR - rules.yaml: rule 'Odd TLS' expression error: 'abc' is not an integer or integer range at line 1 col 13
    port.dst == abc
                ^
```

With `debug=2` (or `-o debug=2`) capture logs, for each expression rule, the parsed
form, the optimized form, the bytecode and, for fieldSet rules, what triggers it.
Capture also logs a warning for an expression that is always true or always false.

## Performance

Expressions compile to bytecode once per load. Conditions on the same field are merged
into one lookup (`f == a || f == b`), constant parts are computed at load, and `&&`/`||`
are reordered so cheap tests (ports, ips, counts) run before expensive ones (regex,
`any()`, `bpf()`). A computed internal field such as `communityId` is only computed once
per check.

## fields: rules

`fields:` rules are compiled into the same bytecode as expressions, with their values
read the way they always have been: values are literal (`foo*` is not a wildcard), and
`,startsWith`/`,endsWith`/`,contains`, `a-b` ranges, cidrs, `ipv4`/`ipv6` and `!field` work
as before. A `fields:` rule is the `&&` of its fields, and each field matches if any
of its values does. They are as fast as expressions or faster, and large lists use hash
tables.

With `log: true`, a `fields:` rule logs `field (value)` for each field that matched, for
example `port.dst (80), host.http (www.example.com)`. `!` fields aren't logged, same as
before.

## Limits

- Parentheses, `!` and `-` can be nested at most 256 deep.
- `any()`/`all()` can be nested 6 deep.
- Lists can be large; 50,000 values load in well under a second.

## Plugin functions

Plugins can add functions in `arkime_plugin_init`, which runs before rules load:

```c
LOCAL gboolean my_is_bad(ArkimeSession_t *session, const ArkimeRulesV2Value_t *args, int nargs,
                         ArkimeRulesV2Value_t *result, gpointer uw)
{
    result->i = g_hash_table_contains(badHosts, args[0].s);
    return TRUE;
}

arkime_rulesv2_register_function("isBadHost", "b:s", 0, my_is_bad, NULL);
```

```
isBadHost(host.http) && port.dst == 80
```

- The signature is the return type, `:`, then up to 8 argument types: `i` integer,
  `f` float, `s` string, `a` ip, and `b` for a boolean return.
- Return `FALSE` for no result, which counts as a missing value.
- Callbacks run on packet threads. Returned strings must stay valid until the rule
  finishes, for example session memory or `arkime_free_later`.
- `ARKIME_RULESV2_FUNC_PURE` means the same args always give the same result without
  the session, so calls with constant args are computed once at load with a NULL
  session.
- A multi value field passed to a function checks each value, like the builtins.

## Notes

- The internal `communityId` field's value has no `1:` version prefix, unlike the
  saved `network.community_id`, so match the part after `1:`.
