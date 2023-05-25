'use strict';

import { countryCodeEmoji } from 'country-code-emoji';
import { applyTemplate } from './applyTemplate';

const filters = require('./filters');

/**
 * A post processor
 *
 * Post processors are used to transform values for display in tidbits.
 * @typedef PostProcessor
 * @type {string|Object<string,*>}
 */

/* generalized post processors --------------------------------- */

// NOTE: assumes that each value ends with \n
const restOfLineFollowing = (data, { param: afterStr }) => {
  if (data == null) { return undefined; }
  const start = data.indexOf(afterStr) + afterStr.length;
  const leftover = data.slice(start);
  const end = leftover.indexOf('\n');
  return data.slice(start, end + start);
};

// step into value using field/path
const pathInto = (value, { param: fieldOrPath }) => {
  const path = Array.isArray(fieldOrPath) ? fieldOrPath : fieldOrPath.split('.');
  for (const p of path) {
    if (!value) { return undefined; }
    value = value[p];
  }
  return value;
};

const template = (value, { param: templateStr }, shared) => {
  return applyTemplate(templateStr, { value, data: shared.data });
};

const wildcardRegex = (value) => {
  return new RegExp(`^${value?.replaceAll('*', '[\\s\\S]*?')}$`);
};

const map = (value, { param: postProcess }, shared) => {
  return value?.map(element => applyPostProcess(postProcess, element, shared));
};

const mapTo = (value, args) => {
  return value?.map(element => pathInto(element, args));
};

// transforms map of objects to an array of objects where the keys are inserted into the objects as [fieldName]
const keyedToArrayWith = (value, { param: fieldName }) => {
  return Object.entries(value).map(([key, val]) => ({ [fieldName]: key, ...val }));
};

const dedupeArray = (value) => {
  return [...new Set(value)];
};

const replaceValues = (value, { param: condition }, shared) => {
  return value.map(v => { return condition; });
};

const filterFunc = (value, { param: condition }, shared) => {
  return value?.filter(element => applyPostProcess(condition, element, shared));
};

const filterOutFunc = (value, { param: condition }, shared) => {
  return value?.filter(element => !applyPostProcess(condition, element, shared));
};

const matchRegex = (value, { param: regex }) => {
  return regex?.test(value);
};

const matchAnyRegex = (value, { param: regexes }) => {
  return regexes?.some(regexValue => regexValue?.test(value));
};

const ifFunc = (value, { param: condition, then, else: other }, shared) => {
  return applyPostProcess(condition, value, shared) ? then : other;
};

/* boolean ----------- */
const not = (value, { param: condition }, shared) => {
  return !applyPostProcess(condition, value, shared);
};

const all = (value, { param: conditions }, shared) => {
  return conditions?.every(condition => applyPostProcess(condition, value, shared));
};

const any = (value, { param: conditions }, shared) => {
  return conditions?.some(condition => applyPostProcess(condition, value, shared));
};

const jsonEquals = (value, { param: to }) => {
  return JSON.stringify(value) === JSON.stringify(to);
};

/* resolvable types are evaluated and replaced with their actual values at runtime */
// get a field from data by path
const dataFunc = (_, { postProcess, ...args }, shared) => { // resolvable
  const fieldValue = pathInto(shared.data, args);
  return applyPostProcess(postProcess, fieldValue, shared);
};

// get a setting by key
const setting = (_, { param: key, postProcess }, shared) => { // resolvable
  return applyPostProcess(postProcess, shared.uiSettings?.[key], shared);
};

// returns the value it is passed -- escapes objects containing resolved keys ('setting', 'data', 'with', 'escapeValue')
const escapeValue = (_, { param: escapedValue }) => { // resolvable
  return escapedValue;
};

// applies postProcess to current value -- used in 'value' key-pair to temporarily modify/access sub-fields
//    for an operation without permanently modifying it
const withFunc = (value, { param: postProcess }, shared) => { // resolvable
  return applyPostProcess(postProcess, value, shared);
};

const customFilters = {
  /* niche operations --------------------------------------------------------------- */
  restOfLineFollowing,
  countryEmoji: countryCodeEmoji,
  wildcardRegex,
  matchAnyRegex,
  /* common operations -------------------------------------------------------------- */
  pathInto,
  flatten: (value) => value?.flat(),
  split: (value, { param: on }) => value?.split(on),
  mapTo,
  map,
  keyedToArrayWith,
  dedupeArray,
  replaceValues,
  setting,
  data: dataFunc,
  escapeValue,
  matchRegex,
  filter: filterFunc,
  filterOut: filterOutFunc,
  removeNullish: (value) => value?.filter(element => element != null),
  trim: (value) => value?.trim(),
  template,
  with: withFunc,
  evaluate: withFunc,
  not,
  all,
  any,
  if: ifFunc,
  jsonEquals,
  equals: (value, { param: other }) => value === other,
  lessThan: (value, { param: other }) => value < other,
  greaterThan: (value, { param: other }) => value > other
};

const resolvePostProcessorArgs = (args, value, shared) => {
  const resolvableTypes = ['data', 'setting', 'escapeValue', 'with', 'evaluate'];
  const resolvedArgs = { ...args };
  for (const [key, arg] of Object.entries(args)) {
    // check for resolvable type
    if (Object.keys(arg)?.some(argKey => resolvableTypes.includes(argKey))) {
      // resolve value
      resolvedArgs[key] = applyPostProcess(arg, value, shared);
    }
  }
  return resolvedArgs;
};

/**
 * Runs a value through a set of named functions or 'filters'
 *   parameterless PostProcessors are strings
 *   object PostProcessors are identified by the key for their primary argument ('param'),
 *     with the option of additional arguments
 *
 * @param {PostProcessor|PostProcessor[]} postProcess
 * @param {*} value
 * @param {Object<string,*>} shared
 * @returns {*} the formatted displayValue (or intermediate value during recursive post-processing)
 */
export const applyPostProcess = (postProcess, value, shared = {}) => {
  shared.data ??= {};
  shared.uiSettings ??= {};

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

        // resolve values from settings, data, or escaping
        args = resolvePostProcessorArgs(args, value, shared);
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
      value = customFilterFunc(inputValue, args, shared);
    } else if (defaultFilterFunc) {
      value = defaultFilterFunc(value);
    } else {
      console.warn(`No such postProcess ${postProcessorName}`);
    }
  }
  return value;
};
