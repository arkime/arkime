/******************************************************************************/
/* bigint.js -- jsdoc plugin so npm run *:doc works on files with BigInt literals
 *
 * Copyright Andy Wick
 * SPDX-License-Identifier: Apache-2.0
 */
'use strict';

// jsdoc keeps the value of a literal initializer like "let big = 0n", and its
// JSON dump throws on a BigInt, so store it as a string instead
exports.handlers = {
  newDoclet ({ doclet }) {
    const code = doclet.meta?.code;
    if (typeof code?.value === 'bigint') {
      code.value = `${code.value}n`;
    }
  }
};
