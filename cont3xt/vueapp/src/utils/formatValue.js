import dr from 'defang-refang';
import { dateString, reDateString } from './filters';

/**
 * Finds the specified value in data and returns the value in its most-formatted usable form
 * i.e. the types 'array' and 'table' return arrays and objects, respectively,
 * while more simple types return formatted strings (since their underlying values are not needed).
 * @param data the object in which to search for the value specified by field
 * @param field the object that describes the value's type and location in data
 * @returns {string|array|object}
 */
export const formatValue = (data, field) => {
  let value = JSON.parse(JSON.stringify(data));

  for (const p of field.path) {
    if (!value) {
      console.warn(`Can't resolve path: ${field.path.join('.')}`);
      return '';
    }
    value = value[p];
  }

  if (field.defang) {
    value = dr.defang(value);
  }

  const arrayLike = field.type === 'array' || (field.type === 'table' && Array.isArray(value));

  if (arrayLike && value != null) {
    // map arrays of objects to flat arrays when a selectPath exists
    if (field.selectPath !== undefined) {
      for (const p of field.selectPath) {
        value = value.map(element => element != null ? element[p] : undefined);
      }
    }

    // nullish filtering for arrays and tables
    if (field.filterEmpty) {
      value = value.filter(element => element != null && element !== '' && element !== []);
    }
  }

  // don't show empty lists or strings (empty tables remain as [] due to auto-collapse behavior)
  if ((field.type !== 'json' && field.type !== 'table') && value && value.length === 0) {
    // ignores ms because it's a number and value.length is undefined
    value = undefined;
  }

  switch (field.type) {
  case 'ms':
    return dateString(value);
  case 'seconds':
    return dateString(value * 1000);
  case 'date':
    return reDateString(value);
  case 'json':
    return JSON.stringify(value, null, 2);
  }

  return value;
};
