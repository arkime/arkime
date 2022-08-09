'use strict';

import { countryCodeEmoji } from 'country-code-emoji';
import { applyTemplate } from './applyTemplate';

const filters = require('./filters');

/* integration-specific operations -------------------------------------- */

// parse the VT Domain whois field for values
// NOTE: assumes that each value ends with \n
const getVTDomainField = (data, fStr) => {
  if (data == null) { return undefined; }
  const start = data.indexOf(fStr) + fStr.length;
  const leftover = data.slice(start);
  const end = leftover.indexOf('\n');
  return data.slice(start, end + start);
};

/* generalized operations --------------------------------- */
const pathInto = (value, { param: fieldOrPath }) => {
  const path = Array.isArray(fieldOrPath) ? fieldOrPath : fieldOrPath.split('.');
  for (const p of path) {
    if (!value) { return undefined; }
    value = value[p];
  }
  return value;
};

const template = (value, { param: templateStr }, _, data) => {
  return applyTemplate(templateStr, { value, data });
};

const wildcardRegex = (value) => {
  return new RegExp(`^${value?.replaceAll('*', '[\\s\\S]*?')}$`);
};

const map = (value, { param: postProcess }, uiSettings, data) => {
  return value?.map(element => applyPostProcess(postProcess, element, data, uiSettings));
};

const mapTo = (value, args) => {
  return value?.map(element => pathInto(element, args));
};

const filterFunc = (value, { param: condition }, uiSettings, data) => {
  return value?.filter(element => applyPostProcess(condition, element, data, uiSettings));
};

const filterOutFunc = (value, { param: condition }, uiSettings, data) => {
  return value?.filter(element => !applyPostProcess(condition, element, data, uiSettings));
};

const match = (value, { param: regex }) => {
  return regex?.test(value);
};

const matchAny = (value, { param: regexes }) => {
  return regexes?.some(regexValue => regexValue?.test(value));
};

const ifFunc = (value, { param: condition, then, else: other }, uiSettings, data) => {
  return applyPostProcess(condition, value, data, uiSettings) ? then : other;
};

/* boolean ----------- */
const not = (value, { param: condition }, uiSettings, data) => {
  return !applyPostProcess(condition, value, data, uiSettings);
};

const all = (value, { param: conditions }, uiSettings, data) => {
  return conditions?.every(condition => applyPostProcess(condition, value, data, uiSettings));
};

const any = (value, { param: conditions }, uiSettings, data) => {
  return conditions?.some(condition => applyPostProcess(condition, value, data, uiSettings));
};

const jsonEquals = (value, { param: to }) => {
  return JSON.stringify(value) === JSON.stringify(to);
};

/* resolvable types are evaluated and replaced with their actual values at runtime */
const dataFunc = (_, { postProcess, ...args }, uiSettings, data) => { // resolvable
  const fieldValue = pathInto(data, args);
  return applyPostProcess(postProcess, fieldValue, data, uiSettings);
};

const setting = (_, { param: key, postProcess }, uiSettings, data) => { // resolvable
  return applyPostProcess(postProcess, uiSettings?.[key], data, uiSettings);
};

const escapeValue = (_, { param: escapedValue }) => { // resolvable
  return escapedValue;
};

const withFunc = (value, { param: postProcess }, uiSettings, data) => {
  return applyPostProcess(postProcess, value, uiSettings, data);
};

const customFilters = {
  /* integration-specific operations ------------------------------------------------ */
  getVTDomainField: (value, { param: field }) => getVTDomainField(value, field),
  /* niche operations --------------------------------------------------------------- */
  countryEmoji: countryCodeEmoji,
  wildcardRegex,
  matchAny,
  /* common operations -------------------------------------------------------------- */
  pathInto,
  flatten: (value) => value?.flat(),
  split: (value, { param: on }) => value?.split(on),
  mapTo,
  map,
  setting,
  data: dataFunc,
  escapeValue,
  match,
  filter: filterFunc,
  filterOut: filterOutFunc,
  removeNullish: (value) => value?.filter(element => element != null),
  trim: (value) => value?.trim(),
  template,
  with: withFunc,
  not,
  all,
  any,
  if: ifFunc,
  jsonEquals,
  equals: (value, { param: other }) => value === other,
  lessThan: (value, { param: other }) => value < other,
  greaterThan: (value, { param: other }) => value > other
};

const resolvePostProcessorArgs = (args, value, data, uiSettings) => {
  const resolvableTypes = ['data', 'setting', 'escapeValue', 'with'];
  const resolvedArgs = { ...args };
  for (const [key, arg] of Object.entries(args)) {
    // check for resolvable type
    if (Object.keys(arg)?.some(argKey => resolvableTypes.includes(argKey))) {
      // resolve value
      resolvedArgs[key] = applyPostProcess(arg, value, data, uiSettings);
    }
  }
  return resolvedArgs;
};

export const applyPostProcess = (postProcess, value, data, uiSettings) => {
  if (!postProcess) { return value; }
  const postProcessors = Array.isArray(postProcess) ? postProcess : [postProcess]; // ensure array
  for (const postProcessor of postProcessors) {
    let postProcessorName = postProcessor;
    let args = {};
    // object processors (processors with one or more field(s))
    if (typeof postProcessor !== 'string') {
      // check if the (object) post-processor has a key matching the name of an existing post-processor
      const argKeys = Object.keys(postProcessor);
      const possiblePostProcessorName = argKeys?.find(key => customFilters[key]);
      if (typeof possiblePostProcessorName === 'string') {
        postProcessorName = possiblePostProcessorName;

        // the arg specifying the processor's name is renamed to 'param'
        args = { ...postProcessor, param: postProcessor[possiblePostProcessorName] };
        delete args[possiblePostProcessorName];

        // resolve values from settings, data, or overriding/escaping
        args = resolvePostProcessorArgs(args, value, data, uiSettings);
      } else {
        // not a valid postProcessor -- stringified for better debugging
        console.warn(`Invalid postProcess ${JSON.stringify(postProcessor)}`);
        continue;
      }
    }

    // in order of preference: first try using a custom filter, then an existing vue-filter, or none (no filter found)
    const customFilterFunc = customFilters[postProcessorName];
    const defaultFilterFunc = filters[postProcessorName];
    if (customFilterFunc) {
      // value used in a post-processor can be overridden with the 'value' field
      const inputValue = Object.keys(args)?.includes('value') ? args.value : value;
      value = customFilterFunc(inputValue, args, uiSettings, data);
    } else if (defaultFilterFunc) {
      value = defaultFilterFunc(value);
    } else {
      console.warn(`No such postProcess ${postProcessorName}`);
    }
  }
  return value;
};
