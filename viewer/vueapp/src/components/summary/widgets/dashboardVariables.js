/*
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0

Dashboard variables (prototype): each control widget picks values of one field
and names them as a variable. The picked values live in the url
(?var.<name>=a&var.<name>=b) so a filtered dashboard can be linked. A control
can filter every widget (its field == the values, ANDed with the search), and
${name} in a widget's title, its filter or the dashboard filter is replaced
with the values in the browser, so the server only ever sees a plain
expression. ${...} can't be confused with a $shortcut, and a { left in an
expression is a parse error rather than a silent miss.
*/
import { buildListExpression } from '@common/vueFilters.js';

export const VARIABLE_PARAM_PREFIX = 'var.';
// __proto__ would set the variables object's prototype rather than a key
export const VARIABLE_NAME_RE = /^(?!__proto__$)[A-Za-z_][A-Za-z0-9_]{0,39}$/;
const VARIABLE_REF_RE = /\$\{([A-Za-z_][A-Za-z0-9_]{0,39})\}/g;
// a field compared to a variable: field == ${name} (or = or !=)
const COMPARE_REF_RE = /([A-Za-z0-9_.:-]+)\s*(==|!=|=)\s*\$\{([A-Za-z_][A-Za-z0-9_]{0,39})\}/g;
// the most values one control can carry into the url and the search
export const MAX_VARIABLE_VALUES = 50;

/** A variable name made from a field expression: ip.src -> ip_src. */
export const defaultVariableName = (fieldExp) => {
  const varName = String(fieldExp || '').replace(/[^A-Za-z0-9_]+/g, '_').replace(/^_+|_+$/g, '').slice(0, 40);
  if (!varName) { return ''; }
  return /^[A-Za-z_]/.test(varName) ? varName : `v_${varName}`.slice(0, 40);
};

/** A control widget's variable: its own name when valid, else one from its field. */
export const controlVariable = (widget) => (VARIABLE_NAME_RE.test(widget?.variable || '') ? widget.variable : defaultVariableName(widget?.field));

/** The values the url picks for a variable (repeated ?var.<name> params). */
export const urlVariableValues = (query, varName) => {
  const v = query?.[`${VARIABLE_PARAM_PREFIX}${varName}`];
  const list = Array.isArray(v) ? v : (typeof v === 'string' ? [v] : []);
  return list.filter(s => typeof s === 'string' && s !== '').slice(0, MAX_VARIABLE_VALUES);
};

/**
 * The dashboard's variables from its control widgets and the url:
 * { name: { field, applyAll, values, owner (the control's widget id) } }.
 * The first control to claim a name owns it; a later one with the same name
 * says so rather than pick values of its field into another's variable.
 */
export const dashboardVariables = (widgets, query) => {
  const vars = {};
  for (const w of widgets || []) {
    if (w?.viewMode !== 'control' || !w.field) { continue; }
    const varName = controlVariable(w);
    if (!VARIABLE_NAME_RE.test(varName) || Object.hasOwn(vars, varName)) { continue; }
    vars[varName] = { field: w.field, applyAll: w.applyAll !== false, values: urlVariableValues(query, varName), owner: w.id };
  }
  return vars;
};

/** The variables with one of them as if nothing were picked (a control's
 *  own value list isn't narrowed by what it picked, however it is used). */
export const withoutPicks = (vars, varName) => (varName && Object.hasOwn(vars || {}, varName)
  ? { ...vars, [varName]: { ...vars[varName], values: [] } }
  : vars);

/** The url query without the picks of some variables (a renamed or removed
 *  control), or of every variable when none are named. */
export const withoutVariableParams = (query, names) => {
  const out = { ...(query || {}) };
  const keys = names ? names.map(n => `${VARIABLE_PARAM_PREFIX}${n}`) : Object.keys(out).filter(k => k.startsWith(VARIABLE_PARAM_PREFIX));
  for (const key of keys) { delete out[key]; }
  return out;
};

/** The filter a control adds to every widget: field == the picked values. */
export const controlFilter = (field, values) => buildListExpression(field, values, '==');

/**
 * The filters of the controls that filter every widget and have values,
 * leaving out one variable (a control's own value list isn't narrowed by
 * what it picked).
 * @returns {string[]}
 */
export const controlFilters = (vars, exclude) => Object.entries(vars || {})
  .filter(([varName, v]) => varName !== exclude && v.applyAll && v.values.length)
  .map(([, v]) => controlFilter(v.field, v.values));

/** True when the text uses ${name} for some variable. */
export const usesVariables = (text) => typeof text === 'string' && /\$\{[A-Za-z_]/.test(text);

// which characters of an expression are inside a "quoted" or /regex/ value,
// where nothing is a variable, a paren or an &&. As in the expression lexer,
// a / only starts a regex at the start of a value (10.0.0.0/8 is no regex)
const literalMask = (exp) => {
  const mask = new Array(exp.length).fill(false);
  let closer = '';
  let prev = '';
  for (let i = 0; i < exp.length; i++) {
    const c = exp[i];
    if (closer) {
      mask[i] = true;
      if (c === '\\') { mask[i + 1] = true; i++; } else if (c === closer) { closer = ''; prev = c; }
      continue;
    }
    if (c === '"' || (c === '/' && (prev === '' || '=!<>&|([,'.includes(prev)))) {
      closer = c;
      mask[i] = true;
    }
    if (!/\s/.test(c)) { prev = c; }
  }
  return mask;
};

// split an expression at its top level && (outside values, lists and parens);
// one with a top level || stays whole, && binding tighter than it
const splitAnd = (exp) => {
  const mask = literalMask(exp);
  const parts = [];
  let depth = 0;
  let start = 0;
  for (let i = 0; i < exp.length; i++) {
    if (mask[i]) { continue; }
    const c = exp[i];
    if (depth === 0 && c === '|' && exp[i + 1] === '|') { return [exp.trim()]; }
    if (c === '(' || c === '[') { depth++; } else if (c === ')' || c === ']') { depth--; } else if (depth === 0 && c === '&' && exp[i + 1] === '&') {
      parts.push(exp.slice(start, i));
      start = i + 2;
      i++;
    }
  }
  parts.push(exp.slice(start));
  return parts.map(p => p.trim()).filter(Boolean);
};

// the ${name} references outside an expression's quoted and regex values
const variableRefs = (exp) => {
  const mask = literalMask(exp);
  return [...exp.matchAll(VARIABLE_REF_RE)].filter(m => !mask[m.index]).map(m => m[1]);
};

/**
 * An expression with each ${name} replaced by its variable's values. A
 * top-level && clause using a variable with nothing picked ("All") is left
 * out, so `node == ${node} && protocols == tls` searches every node until one
 * is picked. An unknown ${name} is left as is (and fails to parse, saying so).
 * @returns {string|undefined} undefined when nothing is left
 */
export const substituteExpression = (exp, vars) => {
  if (!usesVariables(exp)) { return exp || undefined; }
  const unset = (varName) => Object.hasOwn(vars || {}, varName) && !vars[varName].values.length;
  const kept = splitAnd(exp).filter(clause => !variableRefs(clause).some(unset));
  if (!kept.length) { return undefined; }
  // field == ${name} becomes the values' list expression, quoted and split
  // like the Sessions page's own; a ${name} anywhere else, or inside a
  // quoted or regex value, is left as is
  const out = kept.join(' && ');
  const mask = literalMask(out);
  return out.replace(COMPARE_REF_RE, (m, field, op, varName, offset) => (Object.hasOwn(vars || {}, varName) && !mask[offset + m.indexOf('${')]
    ? buildListExpression(field, vars[varName].values, op === '!=' ? '!=' : '==')
    : m));
};

/** A title with each ${name} replaced by its picked values, or allLabel when none. */
export const substituteTitle = (title, vars, allLabel) => {
  if (!usesVariables(title)) { return title; }
  return title.replace(VARIABLE_REF_RE, (m, varName) => {
    if (!Object.hasOwn(vars || {}, varName)) { return m; }
    const values = vars[varName].values;
    return values.length ? values.join(', ') : allLabel;
  });
};
