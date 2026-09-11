/*
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
*/
'use strict';

// Fixed thresholds for stats resource meters (issue #4085):
//   < 70% green (success), 70–90% amber (warning), > 90% red (error).
// "Free" metrics invert the scale so a low value is bad (e.g. low disk free = red).
export const RESOURCE_THRESHOLDS = { warning: 70, error: 90 };

// Muted theme token used when a metric's data is missing/unknown, so absence of
// data reads as grey rather than a false-healthy green (e.g. multi-cluster
// against older viewers that don't return the new fields).
export const UNKNOWN_COLOR = 'neutral';

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
  if (!isFinite(c) || !isFinite(t) || t <= 0) { return UNKNOWN_COLOR; } // no target/data
  if (c < t * 0.8) { return 'error'; } // meaningfully below target (disk pressure)
  if (c > t * 5) { return 'warning'; } // far above target (idle / under-utilized)
  return 'success'; // at/near target
}

// Worst (reddest) color across a set of { percent, invert } gauges — the card status dot.
export function worstResourceColor (metrics) {
  let worst = -1;
  let color = UNKNOWN_COLOR; // grey until at least one finite metric is seen
  for (const m of metrics) {
    const severity = resourceSeverity(m.percent, m.invert);
    if (isFinite(severity) && severity > worst) {
      worst = severity;
      color = resourceColor(m.percent, m.invert);
    }
  }
  return color;
}
