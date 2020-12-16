'use strict';

exports.getReturnName = (value, type, options) => {
  if (!value) { return type || ''; }
  const split = value.split('-');
  if (split.length > 1) {
    return split[0].trim();
  } else {
    return '';
  }
};

exports.getReturnDescription = (value, options) => {
  if (!value) { return ''; }
  const split = value.split('-');
  if (split.length > 1) {
    return split[1].trim();
  } else {
    return split[0].trim();
  }
};

exports.getKind = (kind, name) => {
  if (kind === 'member' && name[0] === '/') {
    return ' API';
  } else if (kind === 'typedef') {
    return ' Type';
  } else {
    return ` (${kind})`;
  }
};
