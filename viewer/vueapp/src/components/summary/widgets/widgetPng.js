/*
Copyright Andy Wick
SPDX-License-Identifier: Apache-2.0

Saves a dashboard widget's body as a PNG by drawing what is on screen onto a
canvas: each svg (with its computed styles inlined, so theme variables
resolve), each canvas (uPlot), and the html text, backgrounds and borders
around them (legends, metric numbers, table cells).
*/

const SCALE = 2;

// the svg properties whose computed values are copied onto the clone
const SVG_PROPS = [
  'fill', 'fill-opacity', 'stroke', 'stroke-width', 'stroke-opacity', 'stroke-dasharray',
  'stroke-linecap', 'stroke-linejoin', 'opacity', 'font-family', 'font-size', 'font-weight',
  'font-style', 'text-anchor', 'dominant-baseline', 'visibility', 'display'
];

const transparent = (color) => !color || color === 'transparent' || /rgba\([^)]*,\s*0\)$/.test(color);

const isHidden = (el) => {
  if (el.checkVisibility) {
    return !el.checkVisibility({ opacityProperty: true, visibilityProperty: true });
  }
  const style = getComputedStyle(el);
  return style.display === 'none' || style.visibility === 'hidden' || style.opacity === '0';
};

const loadImage = (src) => new Promise((resolve, reject) => {
  const img = new Image();
  img.onload = () => resolve(img);
  img.onerror = reject;
  img.src = src;
});

// an svg as an image at SCALE times its size on screen
const svgImage = async (svg, rect) => {
  const clone = svg.cloneNode(true);
  const from = [svg, ...svg.querySelectorAll('*')];
  const to = [clone, ...clone.querySelectorAll('*')];
  from.forEach((el, i) => {
    const style = getComputedStyle(el);
    to[i].setAttribute('style', SVG_PROPS.map(p => `${p}:${style.getPropertyValue(p)}`).join(';'));
  });
  if (!clone.getAttribute('viewBox')) {
    const w = svg.width?.baseVal?.value || rect.width;
    const h = svg.height?.baseVal?.value || rect.height;
    clone.setAttribute('viewBox', `0 0 ${w} ${h}`);
  }
  clone.setAttribute('width', rect.width * SCALE);
  clone.setAttribute('height', rect.height * SCALE);
  clone.setAttribute('xmlns', 'http://www.w3.org/2000/svg');
  const url = URL.createObjectURL(new Blob([new XMLSerializer().serializeToString(clone)], { type: 'image/svg+xml' }));
  try {
    return await loadImage(url);
  } finally {
    URL.revokeObjectURL(url);
  }
};

const drawBox = (ctx, el, rect, style) => {
  if (!transparent(style.backgroundColor)) {
    ctx.fillStyle = style.backgroundColor;
    const radius = parseFloat(style.borderTopLeftRadius) || 0;
    ctx.beginPath();
    if (radius && ctx.roundRect) {
      ctx.roundRect(rect.x, rect.y, rect.width, rect.height, radius);
    } else {
      ctx.rect(rect.x, rect.y, rect.width, rect.height);
    }
    ctx.fill();
  }
  for (const side of ['Top', 'Right', 'Bottom', 'Left']) {
    const width = parseFloat(style[`border${side}Width`]) || 0;
    const color = style[`border${side}Color`];
    if (!width || style[`border${side}Style`] === 'none' || transparent(color)) { continue; }
    ctx.fillStyle = color;
    if (side === 'Top') { ctx.fillRect(rect.x, rect.y, rect.width, width); }
    if (side === 'Bottom') { ctx.fillRect(rect.x, rect.y + rect.height - width, rect.width, width); }
    if (side === 'Left') { ctx.fillRect(rect.x, rect.y, width, rect.height); }
    if (side === 'Right') { ctx.fillRect(rect.x + rect.width - width, rect.y, width, rect.height); }
  }
};

// a text node line by line: a wrapped node has one rect per line, so its
// words are placed one by one
const drawText = (ctx, node, at) => {
  const el = node.parentElement;
  const style = getComputedStyle(el);
  ctx.font = `${style.fontStyle} ${style.fontWeight} ${style.fontSize} ${style.fontFamily}`;
  ctx.fillStyle = style.color;
  ctx.textBaseline = 'middle';
  const range = document.createRange();
  range.selectNodeContents(node);
  const pieces = range.getClientRects().length > 1
    ? [...node.textContent.matchAll(/\S+/g)].map(m => [m.index, m.index + m[0].length])
    : [[0, node.textContent.length]];

  ctx.save();
  if (style.overflow !== 'visible') {
    const p = at(el.getBoundingClientRect());
    ctx.beginPath();
    ctx.rect(p.x, p.y, p.width, p.height);
    ctx.clip();
  }
  for (const [start, end] of pieces) {
    range.setStart(node, start);
    range.setEnd(node, end);
    const r = range.getClientRects()[0];
    if (!r) { continue; }
    const box = at(r);
    ctx.fillText(node.textContent.slice(start, end).trim(), box.x, box.y + box.height / 2);
  }
  ctx.restore();
};

/**
 * Downloads the element as a PNG named after the title, on the theme's
 * background color.
 */
export async function downloadWidgetPng (root, title, backgroundColor) {
  const box = root.getBoundingClientRect();
  const at = (r) => ({ x: r.left - box.left, y: r.top - box.top, width: r.width, height: r.height });

  const canvas = document.createElement('canvas');
  canvas.width = Math.ceil(box.width * SCALE);
  canvas.height = Math.ceil(box.height * SCALE);
  const ctx = canvas.getContext('2d');
  ctx.scale(SCALE, SCALE);
  ctx.fillStyle = backgroundColor;
  ctx.fillRect(0, 0, box.width, box.height);

  const walk = async (node) => {
    if (node.nodeType === Node.TEXT_NODE) {
      if (node.textContent.trim()) { drawText(ctx, node, at); }
      return;
    }
    // buttons are the widget's controls (a map's zoom) and the u- ones uPlot's
    // cursor and selection, not what it shows
    if (node.nodeType !== Node.ELEMENT_NODE || isHidden(node) || node.matches('button, .v-btn, .u-cursor-pt, .u-cursor-x, .u-cursor-y, .u-select')) { return; }
    const rect = node.getBoundingClientRect();
    if (node instanceof SVGSVGElement) {
      if (rect.width && rect.height) { ctx.drawImage(await svgImage(node, rect), ...Object.values(at(rect))); }
      return;
    }
    if (node instanceof HTMLCanvasElement) {
      if (rect.width && rect.height) { ctx.drawImage(node, ...Object.values(at(rect))); }
      return;
    }
    if (node !== root) { drawBox(ctx, node, at(rect), getComputedStyle(node)); }
    for (const child of node.childNodes) { await walk(child); }
  };
  await walk(root);

  const link = document.createElement('a');
  link.download = `arkime-${String(title || 'widget').replace(/[^A-Za-z0-9_-]+/g, '-').replace(/^-+|-+$/g, '').toLowerCase() || 'widget'}.png`;
  link.href = canvas.toDataURL('image/png');
  link.click();
}
