'use strict';

export const iTypes = ['domain', 'ip', 'url', 'email', 'phone', 'hash', 'text'];

export const iTypeIndexMap = Object.fromEntries(iTypes.map((iType, i) => [iType, i]));

export const iTypeIconMap = {
  domain: 'fa-globe',
  ip: 'fa-map-marker',
  url: 'fa-link',
  email: 'fa-at',
  phone: 'fa-phone',
  hash: 'fa-hashtag',
  text: 'fa-font'
};

export const iTypeColorMap = {
  domain: '#ff8f8f',
  ip: '#ffbe8f',
  url: '#ffec8f',
  email: '#a4ff8f',
  phone: '#8fd8ff',
  hash: '#ab8fff',
  text: '#ff8fdf'
};

export const iTypeColorStyleMap = Object.fromEntries(
  Object.entries(iTypeColorMap).map(([iType, color]) => [
    iType, { color }
  ])
);
