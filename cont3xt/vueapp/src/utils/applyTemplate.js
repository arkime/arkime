/**
 * Fills a templated string from an integration configuration
 * @example
 * // returns 'hi and hello'
 * applyTemplate('<value> and <data.something_else>', {value: 'hi', data: {something: 'hi', something_else: 'hello'}})
 *
 * @param {string} templateStr
 * @param {object} accessibleVars
 * @returns {string} Returns the string with the template applied using the data provided
 */
export const applyTemplate = (templateStr, accessibleVars) => {
  const invalidResult = 'INVALID_CONT3XT_TEMPLATE';

  let outputStr = '';
  let partialTemplate = templateStr;

  while (partialTemplate.length > 0) {
    const [start, stop] = [partialTemplate.indexOf('<'), partialTemplate.indexOf('>')];
    if (start === -1 && stop === -1) { // no more templating to be done
      outputStr += partialTemplate;
      break;
    }

    if (start === -1 || stop === -1) { // invalid -- odd number of openers or closers
      console.warn(`Invalid template: ${templateStr} -- odd number of template openers ('<') or closers ('>')`);
      return invalidResult;
    }

    if (stop < start) { // invalid -- opener ('<') must always come before closer ('>')
      console.warn(`Invalid template: ${templateStr} -- template opener ('<') must precede template opener ('>')`);
      return invalidResult;
    }

    const pathStr = partialTemplate.substring(start + 1, stop);
    if (pathStr.includes('<')) { // invalid -- nested templates
      console.warn(`Invalid template: ${templateStr} -- do not nest templates`);
      return invalidResult;
    }

    const path = pathStr.split('.');
    if (path.length === 0) { // invalid -- template must have path
      console.warn(`Invalid template ${templateStr} -- path required in template`);
      return invalidResult;
    }

    let data = accessibleVars;
    for (const p of path) {
      if (data == null) {
        console.warn(`Can't resolve path: ${path.join('.')} in template ${templateStr}`);
        data = 'undefined';
        break; // break for-loop
      }
      data = data[p];
    }
    outputStr += partialTemplate.substring(0, start) + data;
    partialTemplate = partialTemplate.substring(stop + 1); // cut off used up template
  }
  // use html entities for escaping angle brackets
  outputStr = outputStr.replaceAll('&lt;', '<');
  outputStr = outputStr.replaceAll('&gt;', '>');
  return outputStr;
};
