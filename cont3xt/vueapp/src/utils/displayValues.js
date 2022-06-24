import dr from 'defang-refang';
import { dateString, reDateString } from './filters';

export const findDisplayValue = (data, field) => {
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

  // don't show empty tables, lists, or strings
  if (field.type !== 'json' && value && value.length === 0) {
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
