'use strict';

exports.getReturnName = (value, options) => {
  let split = value.split('-');
  if (split.length > 1) {
    return split[0].trim();
  } else {
    return '';
  }
}

exports.getReturnDescription = (value, options) => {
  let split = value.split('-');
  if (split.length > 1) {
    return split[1].trim();
  } else {
    return split[0].trim();
  }
}
