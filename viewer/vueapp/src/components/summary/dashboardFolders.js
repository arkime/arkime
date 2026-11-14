/*
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0

Dashboard folders: a dashboard keeps its folder in its data (data.folder, so
a v6 viewer just ignores it); a navigation entry may carry it as `folder`. The dropdown and
the navigation widget both group a list by folder with this, so they always
agree on the order: the ungrouped dashboards first, then each folder, every
list sorted by name.
*/

const MAX_FOLDER = 64;

/** A dashboard's folder name ('' when it has none). */
export const folderOf = (d) => {
  const raw = d?.folder ?? d?.data?.folder;
  return typeof raw === 'string' ? raw.trim().slice(0, MAX_FOLDER) : '';
};

const byName = (locale) => (a, b) => String(a.name || '').localeCompare(String(b.name || ''), locale);

/**
 * Groups dashboards by folder.
 * @param {object[]} items - dashboards ({ id, name, folder? | data.folder? })
 * @param {string} [locale] - for the sort
 * @returns {{ top: object[], folders: { name: string, items: object[] }[] }}
 *   the ungrouped dashboards sorted by name, then the folders sorted by name,
 *   each holding its dashboards sorted by name
 */
export function groupDashboards (items, locale) {
  const ungrouped = [];
  const byFolder = new Map();
  for (const d of items || []) {
    const folder = folderOf(d);
    if (!folder) { ungrouped.push(d); continue; }
    if (!byFolder.has(folder)) { byFolder.set(folder, []); }
    byFolder.get(folder).push(d);
  }
  const cmp = byName(locale);
  ungrouped.sort(cmp);
  const folders = [...byFolder.entries()]
    .map(([folderName, list]) => ({ name: folderName, items: list.sort(cmp) }))
    .sort(cmp);
  return { top: ungrouped, folders };
}

/**
 * The rows a flat list renders for a grouped set: a folder row (kind
 * 'folder', with its name and whether it is open) followed by its item rows
 * (kind 'item', indented) when open; ungrouped items come first. A search
 * term keeps only the matching items and opens every folder that has one.
 * @param {object[]} items - dashboards
 * @param {object} opts - { locale, openFolders: Set<string>, term: string }
 */
export function dashboardRows (items, { locale, openFolders, term } = {}) {
  const needle = (term || '').trim().toLowerCase();
  const matches = (d) => !needle || String(d.name || '').toLowerCase().includes(needle);
  const grouped = groupDashboards((items || []).filter(matches), locale);
  const rows = grouped.top.map(d => ({ kind: 'item', key: d.id, item: d, indent: false }));
  for (const f of grouped.folders) {
    const isOpen = needle ? true : !!openFolders?.has(f.name);
    rows.push({ kind: 'folder', key: `folder:${f.name}`, name: f.name, open: isOpen, count: f.items.length });
    if (isOpen) {
      for (const d of f.items) { rows.push({ kind: 'item', key: d.id, item: d, indent: true }); }
    }
  }
  return rows;
}

const STORAGE_KEY = 'arkimeDashboardFolders';

/** The folders the user left open, from this browser's storage. */
export function loadOpenFolders () {
  try {
    const raw = JSON.parse(localStorage.getItem(STORAGE_KEY) || '[]');
    return new Set(Array.isArray(raw) ? raw.filter(f => typeof f === 'string') : []);
  } catch {
    return new Set();
  }
}

/** Remembers the open folders in this browser's storage (best effort). */
export function saveOpenFolders (openSet) {
  try { localStorage.setItem(STORAGE_KEY, JSON.stringify([...openSet])); } catch { /* storage unavailable */ }
}
