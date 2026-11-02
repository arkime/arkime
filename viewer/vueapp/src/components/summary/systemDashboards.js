/*
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0

System dashboards: the ones Arkime ships and the ones an admin added to a
dashboardsDir directory (*.dashboard.json files), read from the viewer. They
are read only and listed beside the user's saved dashboards by the dashboards
dropdown and the navigation widget. A system dashboard's id is
system-<file name>, which the Arkime tab opens from ?dashboard=<id> like a
saved dashboard's.
*/
import { fetchWrapper } from '@common/fetchWrapper.js';

const SYSTEM_PREFIX = 'system-';

/** True for a system dashboard's id. */
export const isSystemId = (id) => typeof id === 'string' && id.startsWith(SYSTEM_PREFIX);

/** The dashboard the Arkime tab opens for a user who hasn't picked a default. */
export const FALLBACK_DEFAULT_ID = `${SYSTEM_PREFIX}overview`;

/**
 * The default dashboard id a user picked, '' when they haven't.
 * @param {object} settings - the user's settings
 * @returns {string}
 */
export const pickedDefaultId = (settings) => {
  const id = settings?.defaultDashboardId;
  return typeof id === 'string' && id ? id : '';
};

/**
 * A user's default dashboard id: the one they picked, else the Overview
 * system dashboard (the Arkime tab keeps a layout of the user's own that was
 * never saved as a dashboard ahead of it, and falls back to the user's own
 * state when the Overview isn't there either).
 * @param {object} settings - the user's settings
 * @returns {string}
 */
export const defaultDashboardId = (settings) => pickedDefaultId(settings) || FALLBACK_DEFAULT_ID;

let cached = null;
let pending = null;

/**
 * The system dashboards, each { id, name, description, data, system: true }.
 * The list is fetched once and kept; refresh re-reads it (the dropdown does,
 * so a file added on the viewer shows up the next time the menu opens).
 * @param {object} [opts] - { refresh: boolean }
 * @returns {Promise<object[]>}
 */
export async function loadSystemDashboards ({ refresh = false } = {}) {
  if (cached && !refresh) { return cached; }
  if (!pending) {
    pending = fetchWrapper({ url: 'api/dashboards/system' })
      .then((response) => {
        cached = Array.isArray(response?.data) ? response.data : [];
        return cached;
      })
      .finally(() => { pending = null; });
  }
  return pending;
}

/** The system dashboard with this id, or undefined. */
export async function systemDashboard (id) {
  if (!isSystemId(id)) { return undefined; }
  const list = await loadSystemDashboards();
  return list.find(d => d.id === id);
}
