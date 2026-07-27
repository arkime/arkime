'use strict';

// Fixed thresholds for stats resource meters (issue #4085):
//   < 70% green (success), 70–90% amber (warning), > 90% red (error).
// "Free" metrics invert the scale so a low value is bad (e.g. low disk free = red).
export const RESOURCE_THRESHOLDS = { warning: 70, error: 90 };

// Map a percent (0-100) to its severity, flipping for inverted "free" metrics.
// Returns NaN when the input isn't a usable number.
export function resourceSeverity (percent, invert = false) {
  if (percent === null || percent === undefined) { return NaN; }
  const p = Number(percent);
  if (!isFinite(p)) { return NaN; }
  const clamped = Math.max(0, Math.min(100, p));
  return invert ? 100 - clamped : clamped;
}

// Vuetify theme token for a metric's bar/gauge color.
export function resourceColor (percent, invert = false) {
  const severity = resourceSeverity(percent, invert);
  if (!isFinite(severity)) { return 'success'; }
  if (severity > RESOURCE_THRESHOLDS.error) { return 'error'; }
  if (severity >= RESOURCE_THRESHOLDS.warning) { return 'warning'; }
  return 'success';
}

// Color a value relative to a deployment/node's own target rather than a fixed
// scale. Used for capture free space, where Arkime holds disk near a configured
// recycle target: at/near target = healthy, below = disk pressure (red), well
// above = under-utilized (amber). Relative bands so it fits any target.
export function targetBandColor (current, target) {
  const c = Number(current);
  const t = Number(target);
  if (!isFinite(c) || !isFinite(t) || t <= 0) { return 'success'; }
  if (c < t * 0.8) { return 'error'; } // meaningfully below target
  if (c > t * 2) { return 'warning'; } // meaningfully above target
  return 'success'; // at/near target
}

// Worst (reddest) color across a set of { percent, invert } gauges — the card status dot.
export function worstResourceColor (metrics) {
  let worst = -1;
  let color = 'success';
  for (const m of metrics) {
    const severity = resourceSeverity(m.percent, m.invert);
    if (isFinite(severity) && severity > worst) {
      worst = severity;
      color = resourceColor(m.percent, m.invert);
    }
  }
  return color;
}
