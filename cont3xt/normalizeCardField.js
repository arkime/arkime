/******************************************************************************/
/* normalizeCardField.js --
 *
 * Copyright Yahoo Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
const normalizeCardField = (inField) => {
  const f = JSON.parse(JSON.stringify(inField));

  if (typeof f === 'string') {
    return {
      label: f,
      path: f.split('.'),
      type: 'string'
    };
  }
  if (f.field === undefined) { f.field = f.label; }
  if (typeof f.field === 'string') { f.path ??= f.field.split('.'); }
  delete f.field;

  if (f.type === undefined) { f.type = 'string'; }

  if (f.type === 'table') {
    f.fields = f.fields.map(subField => normalizeCardField(subField));
  }

  if (f.type === 'array' || f.type === 'table') { // array-like types
    if (f.filterEmpty === undefined) { f.filterEmpty = true; }

    if (f.fieldRoot !== undefined) {
      f.fieldRootPath ??= f.fieldRoot.split('.');
      delete f.fieldRoot;
    }
  }

  // disable filtering if not searchable
  f.noSearch ??= f.type === 'externalLink';

  return f;
};

module.exports = normalizeCardField;
